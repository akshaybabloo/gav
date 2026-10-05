#include "mediasession.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QWindow>
#include <QtConcurrent/QtConcurrentRun>

#include <spdlog/spdlog.h>

#ifndef GAV_HAS_MEDIA_SESSION_BACKEND
std::unique_ptr<MediaSessionBackend> createPlatformMediaSessionBackend(MediaSession *) { return nullptr; }
#endif

MediaSession::MediaSession(QObject *parent) : QObject(parent) { m_artworkWriter.setMaxThreadCount(1); }

MediaSession::~MediaSession() { m_artworkWriter.waitForDone(); }

const MediaSessionState &MediaSession::state() const { return m_state; }

void MediaSession::initialise(QWindow *window) {
    if (m_backend) {
        return;
    }
    std::unique_ptr<MediaSessionBackend> backend = createPlatformMediaSessionBackend(this);
    if (!backend) {
        return;
    }
    if (!backend->initialise(window)) {
        spdlog::warn("Media session integration is unavailable; media keys and now-playing are disabled");
        return;
    }
    m_backend = std::move(backend);
    m_backend->stateChanged(m_state);
}

void MediaSession::notifyState() {
    if (m_backend) {
        m_backend->stateChanged(m_state);
    }
    emit stateChanged();
}

#define GAV_SESSION_PROPERTY(Type, getter, setter, member)                                                                                 \
    Type MediaSession::getter() const { return m_state.member; }                                                                          \
    void MediaSession::setter(Type value) {                                                                                               \
        if (m_state.member == value) {                                                                                                    \
            return;                                                                                                                       \
        }                                                                                                                                 \
        m_state.member = value;                                                                                                           \
        notifyState();                                                                                                                    \
    }

QString MediaSession::playbackStatus() const { return m_state.playbackStatus; }

void MediaSession::setPlaybackStatus(const QString &status) {
    const QString normalised = status == QLatin1String("Playing") || status == QLatin1String("Paused") ? status : QStringLiteral("Stopped");
    if (m_state.playbackStatus == normalised) {
        return;
    }
    m_state.playbackStatus = normalised;
    m_positionClock.invalidate();
    notifyState();
}

QString MediaSession::title() const { return m_state.title; }

void MediaSession::setTitle(const QString &title) {
    if (m_state.title == title) {
        return;
    }
    m_state.title = title;
    notifyState();
}

QString MediaSession::artist() const { return m_state.artist; }

void MediaSession::setArtist(const QString &artist) {
    if (m_state.artist == artist) {
        return;
    }
    m_state.artist = artist;
    notifyState();
}

QString MediaSession::album() const { return m_state.album; }

void MediaSession::setAlbum(const QString &album) {
    if (m_state.album == album) {
        return;
    }
    m_state.album = album;
    notifyState();
}

GAV_SESSION_PROPERTY(qint64, durationMs, setDurationMs, durationMs)
GAV_SESSION_PROPERTY(bool, hasMedia, setHasMedia, hasMedia)
GAV_SESSION_PROPERTY(bool, hasVideo, setHasVideo, hasVideo)
GAV_SESSION_PROPERTY(bool, canGoNext, setCanGoNext, canGoNext)
GAV_SESSION_PROPERTY(bool, canGoPrevious, setCanGoPrevious, canGoPrevious)
GAV_SESSION_PROPERTY(bool, canSeek, setCanSeek, canSeek)
GAV_SESSION_PROPERTY(qreal, volume, setVolume, volume)
GAV_SESSION_PROPERTY(qreal, rate, setRate, rate)

#undef GAV_SESSION_PROPERTY

qint64 MediaSession::positionMs() const { return m_state.positionMs; }

void MediaSession::setPositionMs(qint64 positionMs) {
    if (m_state.positionMs == positionMs) {
        return;
    }
    const qint64 delta = positionMs - m_state.positionMs;
    const bool playing = m_state.playbackStatus == QLatin1String("Playing");
    const qint64 expected = playing && m_positionClock.isValid() ? qRound64(m_positionClock.elapsed() * m_state.rate) : 0;
    const bool seeked = qAbs(delta - expected) > seekJumpThresholdMs;

    m_state.positionMs = positionMs;
    m_positionClock.start();
    if (m_backend) {
        m_backend->positionChanged(m_state, seeked);
    }
    emit positionMsChanged();
}

QUrl MediaSession::artworkUrl() const { return m_state.artworkUrl; }

void MediaSession::setArtwork(const QImage &image) {
    const quint64 generation = ++m_artworkGeneration;
    if (image.isNull()) {
        if (!m_state.artworkUrl.isEmpty()) {
            m_state.artworkUrl.clear();
            notifyState();
        }
        return;
    }

    const QString directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString path = QDir(directory).filePath(QStringLiteral("nowplaying-%1.png").arg(generation));
    const QString published = m_state.artworkUrl.toLocalFile();
    QtConcurrent::run(&m_artworkWriter, [image, directory, path, published] {
        QDir().mkpath(directory);
        const QDir cache(directory);
        const QStringList stale = cache.entryList({QStringLiteral("nowplaying-*.png")}, QDir::Files);
        for (const QString &name : stale) {
            const QString candidate = cache.filePath(name);
            if (candidate != published && candidate != path) {
                QFile::remove(candidate);
            }
        }
        QSaveFile file(path);
        return file.open(QIODevice::WriteOnly) && image.save(&file, "PNG") && file.commit();
    }).then(this, [this, generation, path](bool saved) {
        if (generation != m_artworkGeneration) {
            return;
        }
        m_state.artworkUrl = saved ? QUrl::fromLocalFile(path) : QUrl();
        notifyState();
    });
}

void MediaSession::requestPlay() {
    if (m_state.hasMedia) {
        emit playRequested();
    }
}

void MediaSession::requestPause() {
    if (m_state.hasMedia) {
        emit pauseRequested();
    }
}

void MediaSession::requestPlayPause() {
    if (m_state.hasMedia) {
        emit playPauseRequested();
    }
}

void MediaSession::requestStop() {
    if (m_state.hasMedia) {
        emit stopRequested();
    }
}

void MediaSession::requestNext() {
    if (m_state.hasMedia && m_state.canGoNext) {
        emit nextRequested();
    }
}

void MediaSession::requestPrevious() {
    if (m_state.hasMedia && m_state.canGoPrevious) {
        emit previousRequested();
    }
}

void MediaSession::requestSeek(qint64 positionMs) {
    if (!m_state.hasMedia || !m_state.canSeek) {
        return;
    }
    const qint64 upper = m_state.durationMs > 0 ? m_state.durationMs : positionMs;
    emit seekRequested(qBound<qint64>(0, positionMs, upper));
}

void MediaSession::requestOpenUri(const QUrl &url) {
    const QString scheme = url.scheme().toLower();
    if (url.isValid() && (url.isLocalFile() || scheme == QLatin1String("http") || scheme == QLatin1String("https"))) {
        emit openUriRequested(url);
    }
}

void MediaSession::requestRaise() { emit raiseRequested(); }

void MediaSession::requestQuit() { emit quitRequested(); }
