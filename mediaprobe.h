#ifndef MEDIAPROBE_H
#define MEDIAPROBE_H

#include "probemessage.h"

#include <QList>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>

#include <memory>

class QProcess;
class QTimer;

class MediaProbe : public QObject {
    Q_OBJECT

public:
    static constexpr int helloTimeoutMs = 5000;
    static constexpr int idleTimeoutMs = 30000;
    static constexpr qsizetype maxLineBytes = 16 * 1024 * 1024;

    explicit MediaProbe(QObject *parent = nullptr);
    ~MediaProbe() override;

    void setProgram(const QString &program);

    void start(const QString &mediaPath, const QStringList &subtitleFiles);
    void loadSubtitleFile(const QString &path);
    void stop();

    static QString externalSource(const QString &path);

signals:
    void mediaReady(const ProbeMessage &media);
    void headerReady(const QString &source, const QString &assHeader);
    void eventsReady(const QString &source, const QList<ProbeEvent> &events);
    void sourceEnded(const QString &source);
    void sourceFailed(const QString &source, const QString &detail);
    void failed(const QStringList &sources, const QString &code, const QString &detail);

private:
    struct Run {
        QProcess *process = nullptr;
        QTimer *helloTimer = nullptr;
        QTimer *idleTimer = nullptr;
        QByteArray buffer;
        QByteArray errorBuffer;
        QStringList expected;
        QSet<QString> streaming;
        QSet<QString> finished;
        bool gotHello = false;
        bool killedByWatchdog = false;
        QString errorCode;
        QString errorDetail;
    };

    void launch(const QStringList &arguments, const QStringList &expectedSources);
    void readOutput(Run *run);
    void readErrors(Run *run);
    void handleMessage(Run *run, const ProbeMessage &message, QList<QPair<QString, QList<ProbeEvent>>> &pending);
    void flushEvents(QList<QPair<QString, QList<ProbeEvent>>> &pending);
    void finish(Run *run, bool crashed);
    void killRun(Run *run, const QString &code);
    void removeRun(Run *run);

    QString m_program;
    QList<Run *> m_runs;
    std::unique_ptr<QTemporaryDir> m_fontsDir;
};

#endif // MEDIAPROBE_H
