#include "playlistio.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSslSocket>
#include <QStringDecoder>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>

namespace {

const QLatin1String extinfPrefix("#EXTINF:");
const QLatin1String currentPrefix("#GAV-CURRENT:");

QString decode(QByteArray data) {
    if (data.startsWith("\xEF\xBB\xBF")) {
        data.remove(0, 3);
    }
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    const QString text = utf8.decode(data);
    if (!utf8.hasError()) {
        return text;
    }
    QStringDecoder local(QStringDecoder::System);
    return local.decode(data);
}

bool hasUrlScheme(const QString &line) {
    static const QRegularExpression pattern(QStringLiteral(R"(^[A-Za-z][A-Za-z0-9+.\-]*://)"));
    return pattern.match(line).hasMatch();
}

bool isHttpUrl(const QUrl &url) {
    const QString scheme = url.scheme().toLower();
    return scheme == QLatin1String("http") || scheme == QLatin1String("https");
}

qsizetype titleSeparator(const QString &rest) {
    bool quoted = false;
    for (qsizetype i = 0; i < rest.size(); ++i) {
        if (rest[i] == QLatin1Char('"')) {
            quoted = !quoted;
        } else if (rest[i] == QLatin1Char(',') && !quoted) {
            return i;
        }
    }
    return rest.indexOf(QLatin1Char(','));
}

int parseDuration(const QString &head) {
    const QString token = head.section(QLatin1Char(' '), 0, 0, QString::SectionSkipEmpty);
    bool ok = false;
    const int whole = token.toInt(&ok);
    if (ok) {
        return whole;
    }
    const double fractional = token.toDouble(&ok);
    return ok ? qRound(fractional) : -1;
}

PlaylistReadResult parseLines(const QByteArray &data, const QString &baseDirectory, const QStringList &supportedExtensions, const QUrl &remoteBase) {
    PlaylistReadResult result;
    result.ok = true;

    const QString text = decode(data);
    const QDir base(baseDirectory);

    QString pendingTitle;
    int pendingDuration = -1;
    int originalCurrent = -1;
    int originalIndex = 0;
    int currentMapped = -1;

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(extinfPrefix)) {
            const QString rest = line.mid(extinfPrefix.size());
            const qsizetype comma = titleSeparator(rest);
            pendingDuration = parseDuration(comma >= 0 ? rest.left(comma) : rest);
            pendingTitle = comma >= 0 ? rest.mid(comma + 1).trimmed() : QString();
            continue;
        }
        if (line.startsWith(currentPrefix)) {
            bool ok = false;
            const int index = line.mid(currentPrefix.size()).trimmed().toInt(&ok);
            originalCurrent = ok && index >= 0 ? index : -1;
            continue;
        }
        if (line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const int thisIndex = originalIndex++;
        PlaylistEntry entry{QUrl(), pendingTitle, pendingDuration};
        pendingTitle.clear();
        pendingDuration = -1;

        QString localPath;
        if (remoteBase.isValid()) {
            const QUrl url = hasUrlScheme(line) ? QUrl(line) : remoteBase.resolved(QUrl(line));
            if (!url.isValid() || !isHttpUrl(url)) {
                ++result.skippedUnsupported;
                continue;
            }
            entry.location = url;
        } else if (hasUrlScheme(line)) {
            const QUrl url(line);
            if (isHttpUrl(url)) {
                entry.location = url;
            } else if (url.scheme().toLower() == QLatin1String("file")) {
                localPath = url.toLocalFile();
            } else {
                ++result.skippedUnsupported;
                continue;
            }
        } else {
            const auto resolve = [&base](const QString &path) { return QDir::isAbsolutePath(path) ? path : base.absoluteFilePath(path); };
            localPath = resolve(line);
            if (line.contains(QLatin1Char('\\')) && !QFileInfo::exists(localPath)) {
                QString converted = line;
                converted.replace(QLatin1Char('\\'), QLatin1Char('/'));
                localPath = resolve(converted);
            }
        }

        if (!localPath.isEmpty()) {
            localPath = QDir::cleanPath(localPath);
            const QFileInfo info(localPath);
            if (!supportedExtensions.isEmpty() && !supportedExtensions.contains(info.suffix().toLower())) {
                ++result.skippedUnsupported;
                continue;
            }
            if (!info.exists()) {
                ++result.skippedMissing;
                continue;
            }
            entry.location = QUrl::fromLocalFile(localPath);
        }

        if (thisIndex == originalCurrent) {
            currentMapped = int(result.document.entries.size());
        } else if (originalCurrent >= 0 && thisIndex > originalCurrent && currentMapped < 0) {
            currentMapped = int(result.document.entries.size());
        }
        result.document.entries.append(entry);
    }

    const int count = int(result.document.entries.size());
    if (originalCurrent >= 0 && count > 0) {
        result.document.currentIndex = currentMapped >= 0 ? qMin(currentMapped, count - 1) : count - 1;
    }
    return result;
}

}

namespace PlaylistIO {

PlaylistReadResult parse(const QByteArray &data, const QString &baseDirectory, const QStringList &supportedExtensions) {
    return parseLines(data, baseDirectory, supportedExtensions, QUrl());
}

PlaylistReadResult parseRemote(const QByteArray &data, const QUrl &source, const QUrl &base) {
    if (isHlsPlaylist(data)) {
        PlaylistReadResult result;
        result.ok = true;
        result.document.entries.append({source, QString(), -1});
        return result;
    }
    return parseLines(data, QString(), {}, base);
}

bool isHlsPlaylist(const QByteArray &data) { return data.startsWith("#EXT-X-") || data.contains("\n#EXT-X-"); }

PlaylistReadResult read(const QString &path, const QStringList &supportedExtensions) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        PlaylistReadResult result;
        result.error = file.errorString();
        return result;
    }
    return parse(file.readAll(), QFileInfo(path).absolutePath(), supportedExtensions);
}

QByteArray serialise(const PlaylistDocument &document) {
    QString text = QStringLiteral("#EXTM3U\n");
    if (document.currentIndex >= 0) {
        text += currentPrefix + QString::number(document.currentIndex) + QLatin1Char('\n');
    }
    for (const PlaylistEntry &entry : document.entries) {
        QString title = entry.title;
        title.replace(QLatin1Char('\n'), QLatin1Char(' '));
        text += extinfPrefix + QString::number(entry.durationSec) + QLatin1Char(',') + title + QLatin1Char('\n');
        text += (entry.location.isLocalFile() ? QDir::toNativeSeparators(entry.location.toLocalFile()) : entry.location.toString()) +
                QLatin1Char('\n');
    }
    return text.toUtf8();
}

bool write(const QString &path, const PlaylistDocument &document, QString *error) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    file.write(serialise(document));
    if (!file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    return true;
}

}

PlaylistFiles::PlaylistFiles(QObject *parent) : QObject(parent) { m_writer.setMaxThreadCount(1); }

PlaylistFiles::~PlaylistFiles() { m_writer.waitForDone(); }

QVariantMap PlaylistFiles::toVariant(const PlaylistReadResult &result) {
    QVariantList entries;
    for (const PlaylistEntry &entry : result.document.entries) {
        entries.append(QVariantMap{{QStringLiteral("path"), entry.location.toString()},
                                   {QStringLiteral("title"), entry.title},
                                   {QStringLiteral("durationSec"), entry.durationSec}});
    }
    return {{QStringLiteral("ok"), result.ok},
            {QStringLiteral("error"), result.error},
            {QStringLiteral("entries"), entries},
            {QStringLiteral("currentIndex"), result.document.currentIndex},
            {QStringLiteral("skippedMissing"), result.skippedMissing},
            {QStringLiteral("skippedUnsupported"), result.skippedUnsupported}};
}

PlaylistDocument PlaylistFiles::fromVariant(const QVariantList &items, int currentIndex) {
    PlaylistDocument document;
    for (qsizetype i = 0; i < items.size(); ++i) {
        const QVariantMap item = items[i].toMap();
        const QUrl url(item.value(QStringLiteral("path")).toString());
        if (!url.isValid() || url.isEmpty()) {
            continue;
        }
        const int duration = item.contains(QStringLiteral("durationSec")) ? item.value(QStringLiteral("durationSec")).toInt() : -1;
        const QString title = item.contains(QStringLiteral("title")) ? item.value(QStringLiteral("title")).toString()
                                                                      : item.value(QStringLiteral("name")).toString();
        if (i == currentIndex) {
            document.currentIndex = int(document.entries.size());
        }
        document.entries.append({url, title, duration});
    }
    return document;
}

void PlaylistFiles::load(const QUrl &url, const QStringList &supportedExtensions, const QString &tag) {
    if (isHttpUrl(url)) {
        fetch(url, tag);
        return;
    }
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QtConcurrent::run([path, supportedExtensions] { return PlaylistIO::read(path, supportedExtensions); })
        .then(this, [this, tag](const PlaylistReadResult &result) { emit loaded(tag, toVariant(result)); });
}

void PlaylistFiles::fetch(const QUrl &url, const QString &tag) {
    const auto playAsStream = [this, url, tag] {
        PlaylistReadResult result;
        result.ok = true;
        result.document.entries.append({url, QString(), -1});
        emit loaded(tag, toVariant(result));
    };
    if (url.scheme().toLower() == QLatin1String("https") && !QSslSocket::supportsSsl()) {
        QMetaObject::invokeMethod(this, playAsStream, Qt::QueuedConnection);
        return;
    }

    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
    }
    QNetworkRequest request(url);
    request.setTransferTimeout(remoteTimeoutMs);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64 total) {
        if (received > maxRemoteBytes || total > maxRemoteBytes) {
            reply->setProperty("tooLarge", true);
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, tag, playAsStream] {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::SslHandshakeFailedError) {
            playAsStream();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            PlaylistReadResult result;
            result.error = reply->property("tooLarge").toBool() ? tr("The playlist is too large") : reply->errorString();
            qWarning() << "Could not download playlist" << url << result.error;
            emit loaded(tag, toVariant(result));
            return;
        }
        const QByteArray data = reply->readAll();
        const QUrl base = reply->url();
        QtConcurrent::run([data, url, base] { return PlaylistIO::parseRemote(data, url, base); }).then(this, [this, tag](PlaylistReadResult result) {
            if (result.document.entries.isEmpty() && result.skippedUnsupported == 0) {
                result.ok = false;
                result.error = tr("The address does not contain a playlist");
            }
            emit loaded(tag, toVariant(result));
        });
    });
}

void PlaylistFiles::save(const QUrl &url, const QVariantList &items, int currentIndex, const QString &tag) {
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    const PlaylistDocument document = fromVariant(items, currentIndex);
    QtConcurrent::run(&m_writer, [path, document] { return PlaylistIO::write(path, document); }).then(this, [this, tag](bool ok) {
        emit saved(tag, ok);
    });
}

void PlaylistFiles::remove(const QUrl &url) {
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    m_writer.start([path] { QFile::remove(path); });
}
