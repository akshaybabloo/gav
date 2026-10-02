#include "mediaprobe.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>

#include <spdlog/spdlog.h>

MediaProbe::MediaProbe(QObject *parent) : QObject(parent) {}

MediaProbe::~MediaProbe() { stop(); }

void MediaProbe::setProgram(const QString &program) { m_program = program; }

QString MediaProbe::externalSource(const QString &path) { return QStringLiteral("external:") + QFileInfo(path).absoluteFilePath(); }

void MediaProbe::start(const QString &mediaPath, const QStringList &subtitleFiles) {
    m_fontsDir = std::make_unique<QTemporaryDir>();

    QStringList arguments{QStringLiteral("--probe"), mediaPath};
    if (m_fontsDir->isValid()) {
        arguments << QStringLiteral("--fonts-dir") << m_fontsDir->path();
    }
    QStringList expected;
    for (const QString &file : subtitleFiles) {
        arguments << QStringLiteral("--subtitle-file") << file;
        expected << externalSource(file);
    }
    launch(arguments, expected);
}

void MediaProbe::loadSubtitleFile(const QString &path) {
    launch({QStringLiteral("--subtitle-file"), path}, {externalSource(path)});
}

void MediaProbe::stop() {
    const QList<Run *> runs = m_runs;
    m_runs.clear();
    for (Run *run : runs) {
        run->process->disconnect(this);
        run->process->kill();
        run->process->waitForFinished(1000);
        delete run->process;
        delete run;
    }
    m_fontsDir.reset();
}

void MediaProbe::launch(const QStringList &arguments, const QStringList &expectedSources) {
    auto *run = new Run;
    run->expected = expectedSources;
    run->process = new QProcess(this);
    run->helloTimer = new QTimer(run->process);
    run->idleTimer = new QTimer(run->process);
    run->helloTimer->setSingleShot(true);
    run->helloTimer->setInterval(helloTimeoutMs);
    run->idleTimer->setSingleShot(true);
    run->idleTimer->setInterval(idleTimeoutMs);
    m_runs.append(run);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("GAV_SUBPROCESS"), QStringLiteral("probe"));
    run->process->setProcessEnvironment(environment);
    run->process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(run->helloTimer, &QTimer::timeout, this, [this, run] { killRun(run, QStringLiteral("timeout")); });
    connect(run->idleTimer, &QTimer::timeout, this, [this, run] { killRun(run, QStringLiteral("timeout")); });
    connect(run->process, &QProcess::readyReadStandardOutput, this, [this, run] { readOutput(run); });
    connect(run->process, &QProcess::readyReadStandardError, this, [this, run] { readErrors(run); });
    connect(run->process, &QProcess::finished, this, [this, run](int exitCode, QProcess::ExitStatus status) {
        readOutput(run);
        finish(run, status == QProcess::CrashExit || (exitCode != 0 && run->errorCode.isEmpty()));
    });
    connect(run->process, &QProcess::errorOccurred, this, [this, run](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            run->errorCode = QStringLiteral("internal");
            run->errorDetail = run->process->errorString();
            finish(run, false);
        }
    });

    const QString program = m_program.isEmpty() ? QCoreApplication::applicationFilePath() : m_program;
    spdlog::debug("Starting media probe: {} {}", program.toStdString(), arguments.join(QLatin1Char(' ')).toStdString());
    run->helloTimer->start();
    run->idleTimer->start();
    run->process->start(program, arguments);
}

void MediaProbe::readErrors(Run *run) {
    run->errorBuffer.append(run->process->readAllStandardError());
    qsizetype newline;
    while ((newline = run->errorBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = run->errorBuffer.left(newline).trimmed();
        run->errorBuffer.remove(0, newline + 1);
        if (!line.isEmpty()) {
            spdlog::debug("probe: {}", line.toStdString());
        }
    }
    if (run->errorBuffer.size() > maxLineBytes) {
        run->errorBuffer.clear();
    }
}

void MediaProbe::readOutput(Run *run) {
    if (!m_runs.contains(run)) {
        return;
    }
    run->buffer.append(run->process->readAllStandardOutput());

    QList<QPair<QString, QList<ProbeEvent>>> pending;
    qsizetype newline;
    while ((newline = run->buffer.indexOf('\n')) >= 0) {
        const QByteArray line = run->buffer.left(newline);
        run->buffer.remove(0, newline + 1);
        if (line.trimmed().isEmpty()) {
            continue;
        }
        const ProbeMessage message = ProbeMessage::parse(line);
        if (!message.isValid()) {
            spdlog::debug("Ignoring malformed probe line ({} bytes)", line.size());
            continue;
        }
        run->idleTimer->start();
        handleMessage(run, message, pending);
    }
    flushEvents(pending);

    if (run->buffer.size() > maxLineBytes) {
        killRun(run, QStringLiteral("internal"));
    }
}

void MediaProbe::flushEvents(QList<QPair<QString, QList<ProbeEvent>>> &pending) {
    for (const auto &[source, events] : std::as_const(pending)) {
        emit eventsReady(source, events);
    }
    pending.clear();
}

void MediaProbe::handleMessage(Run *run, const ProbeMessage &message, QList<QPair<QString, QList<ProbeEvent>>> &pending) {
    switch (message.type) {
    case ProbeMessage::Type::Hello:
        run->gotHello = true;
        run->helloTimer->stop();
        break;
    case ProbeMessage::Type::Media:
        flushEvents(pending);
        emit mediaReady(message);
        break;
    case ProbeMessage::Type::Header:
        flushEvents(pending);
        run->streaming.insert(message.source);
        emit headerReady(message.source, message.assHeader);
        break;
    case ProbeMessage::Type::Event:
        if (!run->streaming.contains(message.source)) {
            break;
        }
        if (pending.isEmpty() || pending.last().first != message.source) {
            pending.append({message.source, {}});
        }
        pending.last().second.append(message.event);
        break;
    case ProbeMessage::Type::End:
        flushEvents(pending);
        run->streaming.remove(message.source);
        run->finished.insert(message.source);
        emit sourceEnded(message.source);
        break;
    case ProbeMessage::Type::Warning:
        if (message.code == QLatin1String("source-failed")) {
            flushEvents(pending);
            const QString source = message.detail.section(QLatin1Char('\t'), 0, 0);
            run->streaming.remove(source);
            run->finished.insert(source);
            emit sourceFailed(source, message.detail.section(QLatin1Char('\t'), 1));
        } else {
            spdlog::warn("Media probe warning {}: {}", message.code.toStdString(), message.detail.toStdString());
        }
        break;
    case ProbeMessage::Type::Error:
        run->errorCode = message.code;
        run->errorDetail = message.detail;
        break;
    case ProbeMessage::Type::Progress:
    case ProbeMessage::Type::Unknown:
    case ProbeMessage::Type::Invalid:
        break;
    }
}

void MediaProbe::killRun(Run *run, const QString &code) {
    if (!m_runs.contains(run)) {
        return;
    }
    spdlog::warn("Stopping unresponsive media probe ({})", code.toStdString());
    run->killedByWatchdog = true;
    if (run->errorCode.isEmpty()) {
        run->errorCode = code;
        run->errorDetail = QStringLiteral("The media probe stopped responding");
    }
    run->process->kill();
}

void MediaProbe::finish(Run *run, bool crashed) {
    if (!m_runs.contains(run)) {
        return;
    }

    QStringList affected(run->streaming.begin(), run->streaming.end());
    for (const QString &source : std::as_const(run->expected)) {
        if (!run->finished.contains(source) && !affected.contains(source)) {
            affected.append(source);
        }
    }

    QString code = run->errorCode;
    QString detail = run->errorDetail;
    if (crashed && code.isEmpty()) {
        code = QStringLiteral("crashed");
        detail = run->process->errorString();
    }

    removeRun(run);
    if (!code.isEmpty()) {
        spdlog::warn("Media probe failed ({}): {}", code.toStdString(), detail.toStdString());
        emit failed(affected, code, detail);
    }
}

void MediaProbe::removeRun(Run *run) {
    m_runs.removeAll(run);
    run->helloTimer->stop();
    run->idleTimer->stop();
    run->process->disconnect(this);
    run->process->deleteLater();
    delete run;
}
