#ifndef SUBTITLECONTROLLER_H
#define SUBTITLECONTROLLER_H

#include "hlsmaster.h"
#include "mediaprobe.h"
#include "subtitlerenderer.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QMediaPlayer;

class SubtitleController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by the media player")
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(QString activeTrackId READ activeTrackId NOTIFY activeTrackChanged)
    Q_PROPERTY(QString activeTrackName READ activeTrackName NOTIFY activeTrackChanged)
    Q_PROPERTY(int delay READ delay WRITE setDelay NOTIFY delayChanged)
    Q_PROPERTY(qreal scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(QString preferredLanguage READ preferredLanguage WRITE setPreferredLanguage NOTIFY preferredLanguageChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
    Q_PROPERTY(SubtitleRenderer *renderer READ renderer CONSTANT)

public:
    static constexpr int delayStepMs = 100;
    static constexpr int delayLimitMs = 600000;
    static constexpr qreal scaleMin = 0.5;
    static constexpr qreal scaleMax = 3.0;

    enum class State { Pending, Streaming, Complete, Failed, FallbackPlainText };

    struct Track {
        QString id;
        bool embedded = false;
        int stream = -1;
        QString path;
        QString language;
        QString title;
        QString codec;
        bool isDefault = false;
        bool forced = false;
        bool remote = false;
        bool requested = false;
        State state = State::Pending;
    };

    explicit SubtitleController(QMediaPlayer *player, QObject *parent = nullptr);

    void setSource(const QUrl &source);
    void setStreamTracks(const QList<HlsSubtitle> &subtitles);
    void clear();

    QVariantList tracks() const;
    QString activeTrackId() const;
    QString activeTrackName() const;
    int delay() const;
    void setDelay(int delayMs);
    qreal scale() const;
    void setScale(qreal scale);
    QString preferredLanguage() const;
    void setPreferredLanguage(const QString &language);
    QVariantList chapters() const;
    SubtitleRenderer *renderer() const;

    static QString displayName(const Track &track, int ordinal);
    static QString languageName(const QString &code);

    Q_INVOKABLE void selectTrack(const QString &id);
    Q_INVOKABLE QString cycleTrack();
    Q_INVOKABLE bool loadFile(const QUrl &url);
    Q_INVOKABLE int adjustDelay(int deltaMs);

signals:
    void tracksChanged();
    void activeTrackChanged();
    void delayChanged();
    void scaleChanged();
    void preferredLanguageChanged();
    void chaptersChanged();
    void errorOccurred(const QString &message);

private:
    void onMediaReady(const ProbeMessage &media);
    void onHeaderReady(const QString &source, const QString &assHeader);
    void onEventsReady(const QString &source, const QList<ProbeEvent> &events);
    void onSourceEnded(const QString &source);
    void onSourceFailed(const QString &source, const QString &detail);
    void onProbeFailed(const QStringList &sources, const QString &code, const QString &detail);

    void activate(const QString &id, bool userChoice);
    void applyActiveTrack();
    void markFailed(const QString &source);
    void autoSelect();
    int qtSubtitleIndex(const Track &track) const;
    Track *find(const QString &id);
    const Track *find(const QString &id) const;
    void setState(const QString &id, State state);
    void loadFonts(const QList<ProbeFont> &fonts);

    QMediaPlayer *m_player;
    MediaProbe *m_probe;
    SubtitleRenderer *m_renderer;
    QList<Track> m_tracks;
    QHash<QString, QByteArray> m_headers;
    QHash<QString, QList<ProbeEvent>> m_events;
    QVariantList m_chapters;
    QString m_activeId;
    QString m_mediaPath;
    QString m_sidecarChoice;
    QString m_preferredLanguage;
    int m_delayMs = 0;
    qreal m_scale = 1.0;
    bool m_userChoseTrack = false;
    bool m_mediaKnown = false;
    quint64 m_generation = 0;
};

#endif // SUBTITLECONTROLLER_H
