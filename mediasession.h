#ifndef MEDIASESSION_H
#define MEDIASESSION_H

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QUrl>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

#include <memory>

class MediaSession;

struct MediaSessionState {
    QString playbackStatus = QStringLiteral("Stopped");
    QString title;
    QString artist;
    QString album;
    qint64 durationMs = 0;
    qint64 positionMs = 0;
    QUrl artworkUrl;
    bool hasMedia = false;
    bool hasVideo = false;
    bool canGoNext = false;
    bool canGoPrevious = false;
    bool canSeek = false;
    qreal volume = 1.0;
    qreal rate = 1.0;
};

class MediaSessionBackend {
public:
    virtual ~MediaSessionBackend() = default;

    virtual bool initialise(QWindow *window) = 0;
    virtual void stateChanged(const MediaSessionState &state) = 0;
    virtual void positionChanged(const MediaSessionState &state, bool seeked) = 0;
};

class MediaSession : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString playbackStatus READ playbackStatus WRITE setPlaybackStatus NOTIFY stateChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY stateChanged)
    Q_PROPERTY(QString artist READ artist WRITE setArtist NOTIFY stateChanged)
    Q_PROPERTY(QString album READ album WRITE setAlbum NOTIFY stateChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs WRITE setDurationMs NOTIFY stateChanged)
    Q_PROPERTY(qint64 positionMs READ positionMs WRITE setPositionMs NOTIFY positionMsChanged)
    Q_PROPERTY(QUrl artworkUrl READ artworkUrl NOTIFY stateChanged)
    Q_PROPERTY(bool hasMedia READ hasMedia WRITE setHasMedia NOTIFY stateChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo WRITE setHasVideo NOTIFY stateChanged)
    Q_PROPERTY(bool canGoNext READ canGoNext WRITE setCanGoNext NOTIFY stateChanged)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious WRITE setCanGoPrevious NOTIFY stateChanged)
    Q_PROPERTY(bool canSeek READ canSeek WRITE setCanSeek NOTIFY stateChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY stateChanged)
    Q_PROPERTY(qreal rate READ rate WRITE setRate NOTIFY stateChanged)

public:
    static constexpr qint64 seekJumpThresholdMs = 1000;

    explicit MediaSession(QObject *parent = nullptr);
    ~MediaSession() override;

    const MediaSessionState &state() const;

    QString playbackStatus() const;
    void setPlaybackStatus(const QString &status);
    QString title() const;
    void setTitle(const QString &title);
    QString artist() const;
    void setArtist(const QString &artist);
    QString album() const;
    void setAlbum(const QString &album);
    qint64 durationMs() const;
    void setDurationMs(qint64 durationMs);
    qint64 positionMs() const;
    void setPositionMs(qint64 positionMs);
    QUrl artworkUrl() const;
    bool hasMedia() const;
    void setHasMedia(bool hasMedia);
    bool hasVideo() const;
    void setHasVideo(bool hasVideo);
    bool canGoNext() const;
    void setCanGoNext(bool canGoNext);
    bool canGoPrevious() const;
    void setCanGoPrevious(bool canGoPrevious);
    bool canSeek() const;
    void setCanSeek(bool canSeek);
    qreal volume() const;
    void setVolume(qreal volume);
    qreal rate() const;
    void setRate(qreal rate);

    Q_INVOKABLE void initialise(QWindow *window);
    Q_INVOKABLE void setArtwork(const QImage &image);

    void requestPlay();
    void requestPause();
    void requestPlayPause();
    void requestStop();
    void requestNext();
    void requestPrevious();
    void requestSeek(qint64 positionMs);
    void requestOpenUri(const QUrl &url);
    void requestRaise();
    void requestQuit();

signals:
    void stateChanged();
    void positionMsChanged();
    void playRequested();
    void pauseRequested();
    void playPauseRequested();
    void stopRequested();
    void nextRequested();
    void previousRequested();
    void seekRequested(qint64 positionMs);
    void openUriRequested(const QUrl &url);
    void raiseRequested();
    void quitRequested();

private:
    void notifyState();

    MediaSessionState m_state;
    std::unique_ptr<MediaSessionBackend> m_backend;
    QElapsedTimer m_positionClock;
    QThreadPool m_artworkWriter;
    quint64 m_artworkGeneration = 0;
};

std::unique_ptr<MediaSessionBackend> createPlatformMediaSessionBackend(MediaSession *session);

#endif // MEDIASESSION_H
