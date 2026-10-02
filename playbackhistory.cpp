#include "playbackhistory.h"
#include "playbackutils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include <spdlog/spdlog.h>

#include <algorithm>

namespace {

QString key(const char *name) { return QString::fromLatin1(name); }

void writeAtomically(const QString &path, const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        spdlog::warn("Could not open history file '{}' for writing: {}", path.toStdString(), file.errorString().toStdString());
        return;
    }
    file.write(data);
    if (!file.commit()) {
        spdlog::warn("Could not write history file '{}': {}", path.toStdString(), file.errorString().toStdString());
    }
}

}

PlaybackHistory::PlaybackHistory(QObject *parent) : PlaybackHistory(defaultStoragePath(), parent) {}

PlaybackHistory::PlaybackHistory(const QString &storagePath, QObject *parent) : QObject(parent), m_storagePath(storagePath) {
    m_writer.setMaxThreadCount(1);
    load();
}

PlaybackHistory::~PlaybackHistory() { waitForPendingWrites(); }

QString PlaybackHistory::defaultStoragePath() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("history.json"));
}

QString PlaybackHistory::normalizePath(const QString &pathOrUrl) {
    const QUrl url(pathOrUrl);
    if (url.isLocalFile()) {
        return QDir::cleanPath(url.toLocalFile());
    }
    if (isNetworkUrl(pathOrUrl)) {
        return url.toString();
    }
    return QDir::cleanPath(pathOrUrl);
}

bool PlaybackHistory::isNetworkUrl(const QString &pathOrUrl) {
    const QString scheme = QUrl(pathOrUrl).scheme().toLower();
    return scheme == QLatin1String("http") || scheme == QLatin1String("https");
}

QString PlaybackHistory::storagePath() const { return m_storagePath; }

void PlaybackHistory::waitForPendingWrites() { m_writer.waitForDone(); }

qint64 PlaybackHistory::savedPosition(const QString &path) const {
    const QString normalized = normalizePath(path);
    for (const PositionEntry &entry : m_positions) {
        if (entry.path == normalized) {
            return entry.positionMs;
        }
    }
    return -1;
}

void PlaybackHistory::recordPosition(const QString &path, qint64 positionMs, qint64 durationMs) {
    if (isNetworkUrl(path)) {
        return;
    }
    if (!PlaybackUtils::resumeEligible(positionMs, durationMs)) {
        removePosition(path);
        return;
    }

    const QString normalized = normalizePath(path);
    m_positions.removeIf([&](const PositionEntry &entry) { return entry.path == normalized; });
    m_positions.prepend({normalized, positionMs, durationMs, QDateTime::currentDateTimeUtc()});
    while (m_positions.size() > maxPositions) {
        m_positions.removeLast();
    }
    save();
}

void PlaybackHistory::removePosition(const QString &path) {
    const QString normalized = normalizePath(path);
    if (m_positions.removeIf([&](const PositionEntry &entry) { return entry.path == normalized; }) > 0) {
        save();
    }
}

void PlaybackHistory::recordOpened(const QString &pathOrUrl) {
    const QString normalized = normalizePath(pathOrUrl);
    if (normalized.isEmpty()) {
        return;
    }
    m_recent.removeIf([&](const RecentEntry &entry) { return entry.path == normalized; });
    m_recent.prepend({normalized, QDateTime::currentDateTimeUtc()});
    while (m_recent.size() > maxRecent) {
        m_recent.removeLast();
    }
    save();
    emit recentChanged();
}

QStringList PlaybackHistory::recentFiles() const {
    QStringList result;
    result.reserve(m_recent.size());
    for (const RecentEntry &entry : m_recent) {
        result.append(entry.path);
    }
    return result;
}

void PlaybackHistory::removeRecent(const QString &pathOrUrl) {
    const QString normalized = normalizePath(pathOrUrl);
    if (m_recent.removeIf([&](const RecentEntry &entry) { return entry.path == normalized; }) > 0) {
        save();
        emit recentChanged();
    }
}

void PlaybackHistory::clearRecent() {
    if (m_recent.isEmpty()) {
        return;
    }
    m_recent.clear();
    save();
    emit recentChanged();
}

void PlaybackHistory::clear() {
    m_positions.clear();
    m_recent.clear();
    save();
    emit recentChanged();
}

void PlaybackHistory::load() {
    QFile file(m_storagePath);
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        spdlog::warn("Could not read history file '{}': {}", m_storagePath.toStdString(), file.errorString().toStdString());
        return;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    const QJsonObject root = document.object();
    if (error.error != QJsonParseError::NoError || !document.isObject() || root.value(key("version")).toInt() != formatVersion) {
        const QString backup = m_storagePath + QStringLiteral(".bak");
        spdlog::warn("History file '{}' is unreadable or has an unknown version; moving it to '{}'", m_storagePath.toStdString(),
                     backup.toStdString());
        QFile::remove(backup);
        QFile::rename(m_storagePath, backup);
        return;
    }

    for (const QJsonValue &value : root.value(key("positions")).toArray()) {
        const QJsonObject object = value.toObject();
        PositionEntry entry{object.value(key("path")).toString(), object.value(key("positionMs")).toInteger(),
                            object.value(key("durationMs")).toInteger(),
                            QDateTime::fromString(object.value(key("lastPlayed")).toString(), Qt::ISODateWithMs)};
        if (!entry.path.isEmpty() && entry.positionMs > 0 && m_positions.size() < maxPositions) {
            m_positions.append(entry);
        }
    }
    for (const QJsonValue &value : root.value(key("recent")).toArray()) {
        const QJsonObject object = value.toObject();
        RecentEntry entry{object.value(key("path")).toString(),
                          QDateTime::fromString(object.value(key("lastOpened")).toString(), Qt::ISODateWithMs)};
        if (!entry.path.isEmpty() && m_recent.size() < maxRecent) {
            m_recent.append(entry);
        }
    }

    std::stable_sort(m_positions.begin(), m_positions.end(),
                     [](const PositionEntry &a, const PositionEntry &b) { return a.lastPlayed > b.lastPlayed; });
    std::stable_sort(m_recent.begin(), m_recent.end(), [](const RecentEntry &a, const RecentEntry &b) { return a.lastOpened > b.lastOpened; });
}

void PlaybackHistory::save() {
    QJsonArray positions;
    for (const PositionEntry &entry : m_positions) {
        positions.append(QJsonObject{{key("path"), entry.path},
                                     {key("positionMs"), entry.positionMs},
                                     {key("durationMs"), entry.durationMs},
                                     {key("lastPlayed"), entry.lastPlayed.toString(Qt::ISODateWithMs)}});
    }
    QJsonArray recent;
    for (const RecentEntry &entry : m_recent) {
        recent.append(QJsonObject{{key("path"), entry.path}, {key("lastOpened"), entry.lastOpened.toString(Qt::ISODateWithMs)}});
    }
    const QByteArray data =
        QJsonDocument(QJsonObject{{key("version"), formatVersion}, {key("positions"), positions}, {key("recent"), recent}}).toJson();

    QtConcurrent::run(&m_writer, writeAtomically, m_storagePath, data);
}
