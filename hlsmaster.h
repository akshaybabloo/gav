#ifndef HLSMASTER_H
#define HLSMASTER_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QUrl>

struct HlsVariant {
    QUrl url;
    qint64 bandwidth = 0;
    QSize resolution;
    QString label;
    QString codecs;
};

struct HlsAudioFormat {
    QString group;
    QString label;
};

struct HlsSubtitle {
    QUrl url;
    QString language;
    QString name;
    bool isDefault = false;
    bool forced = false;
};

struct HlsPlayback {
    QUrl url;
    QByteArray manifest;
    qint64 bandwidth = 0;
};

struct HlsMaster {
    struct Entry {
        QUrl url;
        QString group;
        QString line;
        QString codecs;
        qint64 bandwidth = 0;
    };

    QList<HlsVariant> variants;
    QList<HlsAudioFormat> audioFormats;
    QList<HlsSubtitle> subtitles;
    bool separateAudio = false;

    QList<Entry> entries;
    QHash<QString, QStringList> audioLines;
    QHash<QString, bool> separateGroups;
};

namespace Hls {

HlsMaster parseMaster(const QByteArray &data, const QUrl &base);
HlsPlayback playback(const HlsMaster &master, int variant, int audioFormat);
int mediumIndex(const QList<HlsVariant> &variants);
int indexForBitrate(const QList<HlsVariant> &variants, qint64 bitsPerSecond, int maxHeight);
QUrl firstSegment(const QByteArray &mediaPlaylist, const QUrl &base);

}

#endif // HLSMASTER_H
