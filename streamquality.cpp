#include "streamquality.h"

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslSocket>
#include <QTimer>

StreamQuality::StreamQuality(QObject *parent) : QObject(parent) {}

bool StreamQuality::handles(const QUrl &url) {
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) {
        return false;
    }
    const QString path = url.path().toLower();
    return path.endsWith(QLatin1String(".m3u8")) || path.endsWith(QLatin1String(".m3u"));
}

QStringList StreamQuality::labels() const {
    QStringList labels;
    for (const HlsVariant &variant : m_master.variants) {
        labels.append(variant.label);
    }
    return labels;
}

QStringList StreamQuality::audioFormats() const {
    QStringList labels;
    for (const HlsAudioFormat &format : m_master.audioFormats) {
        labels.append(format.label);
    }
    return labels;
}

int StreamQuality::activeAudioFormat() const { return m_master.audioFormats.isEmpty() ? -1 : m_audioFormat; }

int StreamQuality::activeIndex() const { return m_active; }

bool StreamQuality::automatic() const { return m_automatic; }

void StreamQuality::cancel() {
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    const bool hadVariants = !m_master.variants.isEmpty();
    m_source.clear();
    m_master = {};
    m_active = -1;
    m_audioFormat = 0;
    m_automatic = true;
    m_measuredBitsPerSecond = 0;
    if (hadVariants) {
        emit changed();
    }
}

void StreamQuality::resolve(const QUrl &source, int maxHeight) {
    cancel();
    m_source = source;
    m_maxHeight = maxHeight;

    if (source.scheme().toLower() == QLatin1String("https") && !QSslSocket::supportsSsl()) {
        QTimer::singleShot(0, this, [this, source] {
            if (m_source == source) {
                emit resolved({source, {}});
            }
        });
        return;
    }

    fetch(source, [this](QNetworkReply *reply, const QByteArray &data) {
        const HlsMaster master = Hls::parseMaster(data, reply->url());
        if (!master.subtitles.isEmpty()) {
            emit subtitlesFound(master.subtitles);
        }
        if (master.variants.size() < 2) {
            emit resolved({m_source, {}});
            return;
        }
        m_master = master;
        const QUrl medium = m_master.variants[Hls::mediumIndex(m_master.variants)].url;
        fetch(medium, [this](QNetworkReply *reply, const QByteArray &data) {
            const QUrl segment = Hls::firstSegment(data, reply->url());
            if (segment.isValid()) {
                measure(segment);
            } else {
                choose(0);
            }
        });
    });
}

void StreamQuality::fetch(const QUrl &url, Handler handler) {
    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
    }
    QNetworkRequest request(url);
    request.setTransferTimeout(requestTimeoutMs);
    QNetworkReply *reply = m_network->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64 total) {
        if (received > maxPlaylistBytes || total > maxPlaylistBytes) {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, handler] {
        reply->deleteLater();
        if (m_reply != reply) {
            return;
        }
        m_reply = nullptr;
        if (reply->error() == QNetworkReply::NoError) {
            handler(reply, reply->readAll());
            return;
        }
        if (!m_master.variants.isEmpty()) {
            choose(0);
            return;
        }
        if (reply->error() == QNetworkReply::TimeoutError) {
            qWarning() << "Could not read stream playlist" << reply->url() << reply->errorString();
            emit failed(reply->errorString());
            return;
        }
        emit resolved({m_source, {}});
    });
}

void StreamQuality::measure(const QUrl &segment) {
    QNetworkRequest request(segment);
    request.setTransferTimeout(probeDeadlineMs);
    QNetworkReply *reply = m_network->get(request);
    m_reply = reply;
    m_probeClock.start();
    m_probeSamples.clear();
    m_probeBytes = 0;

    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        if (m_reply != reply) {
            return;
        }
        m_probeBytes += reply->readAll().size();
        m_probeSamples.append({m_probeClock.elapsed(), m_probeBytes});
        if (m_probeSamples.last().ms - m_probeSamples.first().ms >= probeWindowMs || m_probeBytes >= maxProbeBytes) {
            finishMeasurement(reply);
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] { finishMeasurement(reply); });
    QTimer::singleShot(probeDeadlineMs, reply, [this, reply] { finishMeasurement(reply); });
}

void StreamQuality::finishMeasurement(QNetworkReply *reply) {
    if (m_reply != reply) {
        return;
    }
    m_reply = nullptr;
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();

    qint64 bitsPerSecond = 0;
    if (!m_probeSamples.isEmpty()) {
        const Sample last = m_probeSamples.last();
        Sample from = m_probeSamples.first();
        for (const Sample &sample : std::as_const(m_probeSamples)) {
            if (last.ms - sample.ms < probeTailMs) {
                break;
            }
            from = sample;
        }
        const Sample first = m_probeSamples.first();
        if (last.ms - first.ms >= 100) {
            bitsPerSecond = (last.bytes - first.bytes) * 8000 / (last.ms - first.ms);
        } else {
            bitsPerSecond = last.bytes * 8000 / qMax<qint64>(1, last.ms);
        }
        if (last.ms - from.ms >= 100) {
            bitsPerSecond = qMax(bitsPerSecond, (last.bytes - from.bytes) * 8000 / (last.ms - from.ms));
        }
    }
    choose(bitsPerSecond);
}

int StreamQuality::automaticIndex() const { return Hls::indexForBitrate(m_master.variants, m_measuredBitsPerSecond, m_maxHeight); }

void StreamQuality::choose(qint64 bitsPerSecond) {
    m_measuredBitsPerSecond = bitsPerSecond;
    m_active = automaticIndex();
    qInfo() << "Stream quality:" << m_master.variants[m_active].label << "of" << labels() << "audio" << audioFormats() << "measured" << bitsPerSecond / 1000 << "kbps";
    emit changed();
    emit resolved(playbackFor(m_active));
}

StreamQuality::Playback StreamQuality::playbackFor(int index) const {
    const HlsPlayback playback = Hls::playback(m_master, index, m_audioFormat);
    if (playback.manifest.isEmpty()) {
        return {playback.url, {}};
    }
    return {m_source.adjusted(QUrl::RemoveQuery | QUrl::RemoveFragment), playback.manifest};
}

StreamQuality::Playback StreamQuality::select(int index) {
    if (m_master.variants.isEmpty() || index >= m_master.variants.size()) {
        return {};
    }
    m_automatic = index < 0;
    const int target = m_automatic ? automaticIndex() : index;
    const bool switched = target != m_active;
    m_active = target;
    emit changed();
    return switched ? playbackFor(m_active) : Playback();
}

StreamQuality::Playback StreamQuality::selectAudioFormat(int index) {
    if (m_active < 0 || index < 0 || index >= m_master.audioFormats.size() || index == m_audioFormat) {
        return {};
    }
    m_audioFormat = index;
    emit changed();
    return playbackFor(m_active);
}
