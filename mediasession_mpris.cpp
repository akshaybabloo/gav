#include "mediasession.h"

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QStringList>
#include <QVariantMap>

#include <spdlog/spdlog.h>

namespace {

const QString objectPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString rootInterface = QStringLiteral("org.mpris.MediaPlayer2");
const QString playerInterface = QStringLiteral("org.mpris.MediaPlayer2.Player");
const QString serviceName = QStringLiteral("org.mpris.MediaPlayer2.gav");

class RootAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit CONSTANT)
    Q_PROPERTY(bool CanRaise READ canRaise CONSTANT)
    Q_PROPERTY(bool HasTrackList READ hasTrackList CONSTANT)
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes CONSTANT)

public:
    RootAdaptor(QObject *parent, MediaSession *session) : QDBusAbstractAdaptor(parent), m_session(session) {}

    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return QStringLiteral("GAV"); }
    QString desktopEntry() const { return QStringLiteral("gav"); }
    QStringList supportedUriSchemes() const { return {QStringLiteral("file"), QStringLiteral("http"), QStringLiteral("https")}; }
    QStringList supportedMimeTypes() const {
        return {QStringLiteral("video/mp4"),       QStringLiteral("video/x-matroska"), QStringLiteral("video/webm"), QStringLiteral("video/quicktime"),
                QStringLiteral("video/x-msvideo"), QStringLiteral("video/x-flv"),      QStringLiteral("video/mpeg"), QStringLiteral("audio/mpeg"),
                QStringLiteral("audio/flac"),      QStringLiteral("audio/ogg"),        QStringLiteral("audio/wav"),  QStringLiteral("audio/aac"),
                QStringLiteral("audio/mp4")};
    }

public slots:
    void Raise() { m_session->requestRaise(); }
    void Quit() { m_session->requestQuit(); }

private:
    MediaSession *m_session;
};

class PlayerAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(double MinimumRate READ minimumRate CONSTANT)
    Q_PROPERTY(double MaximumRate READ maximumRate CONSTANT)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canControl)
    Q_PROPERTY(bool CanPause READ canControl)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl)

public:
    PlayerAdaptor(QObject *parent, MediaSession *session) : QDBusAbstractAdaptor(parent), m_session(session) {}

    void setTrackNumber(int number) { m_trackNumber = number; }

    QString playbackStatus() const { return m_session->state().playbackStatus; }
    qlonglong position() const { return m_session->state().positionMs * 1000; }
    double rate() const { return m_session->state().rate; }
    void setRate(double) {}
    double minimumRate() const { return 0.25; }
    double maximumRate() const { return 2.0; }
    double volume() const { return m_session->state().volume; }
    void setVolume(double) {}
    bool canGoNext() const { return m_session->state().hasMedia && m_session->state().canGoNext; }
    bool canGoPrevious() const { return m_session->state().hasMedia && m_session->state().canGoPrevious; }
    bool canSeek() const { return m_session->state().hasMedia && m_session->state().canSeek; }
    bool canControl() const { return m_session->state().hasMedia; }

    QDBusObjectPath trackId() const {
        return QDBusObjectPath(m_session->state().hasMedia ? QStringLiteral("/org/gav/track/%1").arg(m_trackNumber)
                                                             : QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack"));
    }

    QVariantMap metadata() const {
        const MediaSessionState &state = m_session->state();
        QVariantMap map{{QStringLiteral("mpris:trackid"), QVariant::fromValue(trackId())}};
        if (!state.hasMedia) {
            return map;
        }
        if (state.durationMs > 0) {
            map.insert(QStringLiteral("mpris:length"), static_cast<qlonglong>(state.durationMs * 1000));
        }
        if (!state.title.isEmpty()) {
            map.insert(QStringLiteral("xesam:title"), state.title);
        }
        if (!state.artist.isEmpty()) {
            map.insert(QStringLiteral("xesam:artist"), QStringList{state.artist});
        }
        if (!state.album.isEmpty()) {
            map.insert(QStringLiteral("xesam:album"), state.album);
        }
        if (!state.artworkUrl.isEmpty()) {
            map.insert(QStringLiteral("mpris:artUrl"), state.artworkUrl.toString());
        }
        return map;
    }

public slots:
    void Play() { m_session->requestPlay(); }
    void Pause() { m_session->requestPause(); }
    void PlayPause() { m_session->requestPlayPause(); }
    void Stop() { m_session->requestStop(); }
    void Next() { m_session->requestNext(); }
    void Previous() { m_session->requestPrevious(); }
    void Seek(qlonglong offsetUs) { m_session->requestSeek(m_session->state().positionMs + offsetUs / 1000); }
    void SetPosition(const QDBusObjectPath &track, qlonglong positionUs) {
        if (track == trackId() && positionUs >= 0) {
            m_session->requestSeek(positionUs / 1000);
        }
    }
    void OpenUri(const QString &uri) { m_session->requestOpenUri(QUrl(uri)); }

signals:
    void Seeked(qlonglong positionUs);

private:
    MediaSession *m_session;
    int m_trackNumber = 0;
};

class MprisBackend : public QObject, public MediaSessionBackend {
    Q_OBJECT

public:
    explicit MprisBackend(MediaSession *session) : m_session(session) {
        m_root = new RootAdaptor(this, session);
        m_player = new PlayerAdaptor(this, session);
    }

    ~MprisBackend() override {
        if (m_registered) {
            QDBusConnection bus = QDBusConnection::sessionBus();
            bus.unregisterObject(objectPath);
            bus.unregisterService(m_service);
        }
    }

    bool initialise(QWindow *) override {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            spdlog::warn("No D-Bus session bus; MPRIS is unavailable");
            return false;
        }
        if (!bus.registerObject(objectPath, this, QDBusConnection::ExportAdaptors)) {
            spdlog::warn("Could not register the MPRIS object: {}", bus.lastError().message().toStdString());
            return false;
        }
        m_service = serviceName;
        if (!bus.registerService(m_service)) {
            m_service = QStringLiteral("%1.instance%2").arg(serviceName).arg(QCoreApplication::applicationPid());
            if (!bus.registerService(m_service)) {
                spdlog::warn("Could not register the MPRIS service: {}", bus.lastError().message().toStdString());
                bus.unregisterObject(objectPath);
                return false;
            }
        }
        m_registered = true;
        return true;
    }

    void stateChanged(const MediaSessionState &state) override {
        if (state.title != m_last.title || state.hasMedia != m_last.hasMedia) {
            m_player->setTrackNumber(++m_trackNumber);
        }

        QVariantMap changed;
        if (state.playbackStatus != m_last.playbackStatus) {
            changed.insert(QStringLiteral("PlaybackStatus"), m_player->playbackStatus());
        }
        if (state.title != m_last.title || state.artist != m_last.artist || state.album != m_last.album || state.durationMs != m_last.durationMs ||
            state.artworkUrl != m_last.artworkUrl || state.hasMedia != m_last.hasMedia) {
            changed.insert(QStringLiteral("Metadata"), m_player->metadata());
        }
        if (state.hasMedia != m_last.hasMedia) {
            changed.insert(QStringLiteral("CanPlay"), m_player->canControl());
            changed.insert(QStringLiteral("CanPause"), m_player->canControl());
            changed.insert(QStringLiteral("CanControl"), m_player->canControl());
        }
        if (state.hasMedia != m_last.hasMedia || state.canGoNext != m_last.canGoNext) {
            changed.insert(QStringLiteral("CanGoNext"), m_player->canGoNext());
        }
        if (state.hasMedia != m_last.hasMedia || state.canGoPrevious != m_last.canGoPrevious) {
            changed.insert(QStringLiteral("CanGoPrevious"), m_player->canGoPrevious());
        }
        if (state.hasMedia != m_last.hasMedia || state.canSeek != m_last.canSeek) {
            changed.insert(QStringLiteral("CanSeek"), m_player->canSeek());
        }
        if (!qFuzzyCompare(state.volume + 1.0, m_last.volume + 1.0)) {
            changed.insert(QStringLiteral("Volume"), state.volume);
        }
        if (!qFuzzyCompare(state.rate + 1.0, m_last.rate + 1.0)) {
            changed.insert(QStringLiteral("Rate"), state.rate);
        }
        m_last = state;

        if (!m_registered || changed.isEmpty()) {
            return;
        }
        QDBusMessage message = QDBusMessage::createSignal(objectPath, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"));
        message << playerInterface << changed << QStringList();
        QDBusConnection::sessionBus().send(message);
    }

    void positionChanged(const MediaSessionState &state, bool seeked) override {
        m_last.positionMs = state.positionMs;
        if (seeked && m_registered) {
            emit m_player->Seeked(state.positionMs * 1000);
        }
    }

private:
    MediaSession *m_session;
    RootAdaptor *m_root;
    PlayerAdaptor *m_player;
    MediaSessionState m_last;
    QString m_service;
    int m_trackNumber = 0;
    bool m_registered = false;
};

}

std::unique_ptr<MediaSessionBackend> createPlatformMediaSessionBackend(MediaSession *session) { return std::make_unique<MprisBackend>(session); }

#include "mediasession_mpris.moc"
