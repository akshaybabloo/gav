#ifndef STREAMQUALITY_H
#define STREAMQUALITY_H

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>

#include <functional>

#include "hlsmaster.h"

class QNetworkAccessManager;
class QNetworkReply;

class StreamQuality : public QObject {
    Q_OBJECT

public:
    explicit StreamQuality(QObject *parent = nullptr);

    static bool handles(const QUrl &url);

    struct Playback {
        QUrl url;
        QByteArray manifest;
    };

    void resolve(const QUrl &source, int maxHeight);
    void cancel();
    Playback select(int index);
    Playback selectAudioFormat(int index);

    QStringList labels() const;
    int activeIndex() const;
    bool automatic() const;
    QStringList audioFormats() const;
    int activeAudioFormat() const;

    static constexpr int requestTimeoutMs = 10000;
    static constexpr qint64 maxPlaylistBytes = 4 * 1024 * 1024;
    static constexpr int probeWindowMs = 1500;
    static constexpr int probeTailMs = 750;
    static constexpr int probeDeadlineMs = 4000;
    static constexpr qint64 maxProbeBytes = 4 * 1024 * 1024;

signals:
    void subtitlesFound(const QList<HlsSubtitle> &subtitles);
    void resolved(const StreamQuality::Playback &playback);
    void failed(const QString &error);
    void changed();

private:
    using Handler = std::function<void(QNetworkReply *reply, const QByteArray &data)>;

    void fetch(const QUrl &url, Handler handler);
    void measure(const QUrl &segment);
    void finishMeasurement(QNetworkReply *reply);
    void choose(qint64 bitsPerSecond);
    int automaticIndex() const;
    Playback playbackFor(int index) const;

    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    QUrl m_source;
    HlsMaster m_master;
    int m_active = -1;
    int m_audioFormat = 0;
    int m_maxHeight = 0;
    bool m_automatic = true;
    qint64 m_measuredBitsPerSecond = 0;

    struct Sample {
        qint64 ms = 0;
        qint64 bytes = 0;
    };

    QElapsedTimer m_probeClock;
    QList<Sample> m_probeSamples;
    qint64 m_probeBytes = 0;
};

#endif // STREAMQUALITY_H
