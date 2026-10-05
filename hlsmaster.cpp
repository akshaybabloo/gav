#include "hlsmaster.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace {

const QLatin1String streamInfPrefix("#EXT-X-STREAM-INF:");
const QLatin1String mediaPrefix("#EXT-X-MEDIA:");

QHash<QString, QString> attributes(const QString &list) {
    QHash<QString, QString> result;
    qsizetype position = 0;
    while (position < list.size()) {
        const qsizetype equals = list.indexOf(QLatin1Char('='), position);
        if (equals < 0) {
            break;
        }
        const QString key = list.mid(position, equals - position).trimmed().toUpper();
        QString value;
        qsizetype end = 0;
        if (equals + 1 < list.size() && list[equals + 1] == QLatin1Char('"')) {
            const qsizetype close = list.indexOf(QLatin1Char('"'), equals + 2);
            if (close < 0) {
                value = list.mid(equals + 2);
                end = list.size();
            } else {
                value = list.mid(equals + 2, close - equals - 2);
                end = list.indexOf(QLatin1Char(','), close);
            }
        } else {
            end = list.indexOf(QLatin1Char(','), equals + 1);
            value = list.mid(equals + 1, end < 0 ? -1 : end - equals - 1).trimmed();
        }
        result.insert(key, value);
        if (end < 0) {
            break;
        }
        position = end + 1;
    }
    return result;
}

bool isHttpUrl(const QUrl &url) {
    const QString scheme = url.scheme().toLower();
    return url.isValid() && (scheme == QLatin1String("http") || scheme == QLatin1String("https"));
}

bool isYes(const QString &value) { return value.compare(QLatin1String("YES"), Qt::CaseInsensitive) == 0; }

QSize parseResolution(const QString &text) {
    const QStringList parts = text.toLower().split(QLatin1Char('x'));
    if (parts.size() != 2) {
        return {};
    }
    bool widthOk = false;
    bool heightOk = false;
    const int width = parts[0].toInt(&widthOk);
    const int height = parts[1].toInt(&heightOk);
    return widthOk && heightOk && width > 0 && height > 0 ? QSize(width, height) : QSize();
}

QString bitrateText(qint64 bandwidth) {
    if (bandwidth >= 1000000) {
        return QStringLiteral("%1 Mbps").arg(bandwidth / 1000000.0, 0, 'f', 1);
    }
    return QStringLiteral("%1 kbps").arg(bandwidth / 1000);
}

QString withAbsoluteUri(const QString &line, const QUrl &base, bool *ok) {
    static const QRegularExpression pattern(QStringLiteral("URI=\"([^\"]*)\""));
    const QRegularExpressionMatch match = pattern.match(line);
    *ok = true;
    if (!match.hasMatch()) {
        return line;
    }
    const QUrl url = base.resolved(QUrl(match.captured(1)));
    *ok = isHttpUrl(url);
    QString result = line;
    result.replace(match.capturedStart(1), match.capturedLength(1), QString::fromLatin1(url.toEncoded()));
    return result;
}

QStringList codecList(const QString &codecs) {
    QStringList result;
    const QStringList parts = codecs.toLower().split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        result.append(part.trimmed());
    }
    return result;
}

int videoRank(const QString &codec) {
    if (codec.startsWith(QLatin1String("avc"))) {
        return 0;
    }
    if (codec.startsWith(QLatin1String("hvc")) || codec.startsWith(QLatin1String("hev"))) {
        return 1;
    }
    if (codec.startsWith(QLatin1String("av01")) || codec.startsWith(QLatin1String("vp09"))) {
        return 2;
    }
    if (codec.startsWith(QLatin1String("dvh")) || codec.startsWith(QLatin1String("dva"))) {
        return 3;
    }
    return -1;
}

int audioRank(const QString &codec) {
    if (codec == QLatin1String("mp4a.40.2")) {
        return 0;
    }
    if (codec.startsWith(QLatin1String("mp4a"))) {
        return 1;
    }
    if (codec == QLatin1String("ac-3")) {
        return 2;
    }
    if (codec == QLatin1String("ec-3")) {
        return 3;
    }
    return -1;
}

QString videoCodecOf(const QString &codecs) {
    for (const QString &codec : codecList(codecs)) {
        if (videoRank(codec) >= 0) {
            return codec;
        }
    }
    return {};
}

QString audioCodecOf(const QString &codecs) {
    for (const QString &codec : codecList(codecs)) {
        if (audioRank(codec) >= 0) {
            return codec;
        }
    }
    return {};
}

QString videoCodecName(const QString &codec) {
    switch (videoRank(codec)) {
    case 0:
        return QStringLiteral("H.264");
    case 1:
        return QStringLiteral("HEVC");
    case 2:
        return codec.startsWith(QLatin1String("vp09")) ? QStringLiteral("VP9") : QStringLiteral("AV1");
    case 3:
        return QStringLiteral("Dolby Vision");
    default:
        return {};
    }
}

QString audioFormatName(const QString &codec, const QString &channels) {
    QString name;
    if (codec == QLatin1String("mp4a.40.2")) {
        name = QStringLiteral("AAC");
    } else if (codec == QLatin1String("mp4a.40.5")) {
        name = QStringLiteral("HE-AAC");
    } else if (codec == QLatin1String("mp4a.40.29")) {
        name = QStringLiteral("HE-AAC v2");
    } else if (codec == QLatin1String("ac-3")) {
        name = QStringLiteral("Dolby Digital");
    } else if (codec == QLatin1String("ec-3")) {
        if (channels.contains(QLatin1String("JOC"), Qt::CaseInsensitive)) {
            return QStringLiteral("Dolby Atmos");
        }
        name = QStringLiteral("Dolby Digital Plus");
    }
    const QString count = channels.section(QLatin1Char('/'), 0, 0).trimmed();
    QString layout;
    if (count == QLatin1String("1")) {
        layout = QStringLiteral("mono");
    } else if (count == QLatin1String("2")) {
        layout = QStringLiteral("stereo");
    } else if (count == QLatin1String("6")) {
        layout = QStringLiteral("5.1");
    } else if (count == QLatin1String("8")) {
        layout = QStringLiteral("7.1");
    }
    return name.isEmpty() || layout.isEmpty() ? name + layout : name + QLatin1Char(' ') + layout;
}

template <typename Items>
void disambiguate(Items &items, const QStringList &suffixes) {
    QHash<QString, int> counts;
    for (const auto &item : items) {
        ++counts[item.label];
    }
    for (qsizetype i = 0; i < items.size(); ++i) {
        if (counts.value(items[i].label) > 1 && !suffixes[i].isEmpty()) {
            items[i].label += suffixes[i];
        }
    }
}

int videoCount(const QList<HlsVariant> &variants) {
    const int count = int(std::count_if(variants.begin(), variants.end(), [](const HlsVariant &variant) { return variant.resolution.height() > 0; }));
    return count > 0 ? count : int(variants.size());
}

}

namespace Hls {

HlsMaster parseMaster(const QByteArray &data, const QUrl &base) {
    HlsMaster master;
    QHash<QString, QString> groupChannels;
    QSet<QString> groupsWithDefault;
    QHash<QString, QString> pending;
    QString pendingLine;
    bool hasPending = false;

    const QStringList lines = QString::fromUtf8(data).split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(mediaPrefix)) {
            const QHash<QString, QString> media = attributes(line.mid(mediaPrefix.size()));
            const QString type = media.value(QStringLiteral("TYPE")).toUpper();
            const QString uri = media.value(QStringLiteral("URI"));
            if (type == QLatin1String("AUDIO")) {
                const QString group = media.value(QStringLiteral("GROUP-ID"));
                const bool isDefault = isYes(media.value(QStringLiteral("DEFAULT")));
                const bool leads = isDefault && !groupsWithDefault.contains(group);
                if (leads || !master.separateGroups.contains(group)) {
                    master.separateGroups.insert(group, !uri.isEmpty());
                    groupChannels.insert(group, media.value(QStringLiteral("CHANNELS")));
                }
                bool usable = false;
                const QString absolute = withAbsoluteUri(line, base, &usable);
                if (usable && leads) {
                    master.audioLines[group].prepend(absolute);
                    groupsWithDefault.insert(group);
                } else if (usable) {
                    master.audioLines[group].append(absolute);
                }
            } else if (type == QLatin1String("SUBTITLES") && !uri.isEmpty()) {
                HlsSubtitle subtitle;
                subtitle.url = base.resolved(QUrl(uri));
                subtitle.language = media.value(QStringLiteral("LANGUAGE"));
                subtitle.name = media.value(QStringLiteral("NAME"));
                subtitle.isDefault = isYes(media.value(QStringLiteral("DEFAULT")));
                subtitle.forced = isYes(media.value(QStringLiteral("FORCED")));
                const bool known = std::any_of(master.subtitles.begin(), master.subtitles.end(), [&subtitle](const HlsSubtitle &other) { return other.url == subtitle.url; });
                if (isHttpUrl(subtitle.url) && !known) {
                    master.subtitles.append(subtitle);
                }
            }
            continue;
        }
        if (line.startsWith(streamInfPrefix)) {
            pending = attributes(line.mid(streamInfPrefix.size()));
            pendingLine = line;
            hasPending = true;
            continue;
        }
        if (line.startsWith(QLatin1Char('#')) || !hasPending) {
            continue;
        }
        hasPending = false;

        HlsMaster::Entry entry;
        entry.url = base.resolved(QUrl(line));
        if (!isHttpUrl(entry.url)) {
            continue;
        }
        entry.group = pending.value(QStringLiteral("AUDIO"));
        entry.line = pendingLine;
        entry.codecs = pending.value(QStringLiteral("CODECS"));
        entry.bandwidth = pending.value(QStringLiteral("BANDWIDTH")).toLongLong();
        master.entries.append(entry);

        const bool known = std::any_of(master.variants.begin(), master.variants.end(), [&entry](const HlsVariant &variant) { return variant.url == entry.url; });
        if (!known) {
            HlsVariant variant;
            variant.url = entry.url;
            variant.resolution = parseResolution(pending.value(QStringLiteral("RESOLUTION")));
            variant.codecs = entry.codecs;
            master.variants.append(variant);
        }
    }

    QStringList groups;
    for (const HlsMaster::Entry &entry : std::as_const(master.entries)) {
        if (!entry.group.isEmpty() && !groups.contains(entry.group)) {
            groups.append(entry.group);
        }
        master.separateAudio = master.separateAudio || master.separateGroups.value(entry.group, false);
    }
    const auto groupCodec = [&master](const QString &group) {
        for (const HlsMaster::Entry &entry : std::as_const(master.entries)) {
            if (entry.group == group) {
                return audioCodecOf(entry.codecs);
            }
        }
        return QString();
    };
    std::stable_sort(groups.begin(), groups.end(), [&groupCodec](const QString &a, const QString &b) {
        const int rankA = audioRank(groupCodec(a));
        const int rankB = audioRank(groupCodec(b));
        return (rankA < 0 ? 1 : rankA) < (rankB < 0 ? 1 : rankB);
    });
    QStringList groupSuffixes;
    for (const QString &group : std::as_const(groups)) {
        const QString name = audioFormatName(groupCodec(group), groupChannels.value(group));
        master.audioFormats.append({group, name.isEmpty() ? group : name});
        groupSuffixes.append(QStringLiteral(" (%1)").arg(group));
    }
    disambiguate(master.audioFormats, groupSuffixes);

    for (HlsVariant &variant : master.variants) {
        variant.bandwidth = 0;
        for (const HlsMaster::Entry &entry : std::as_const(master.entries)) {
            if (entry.url != variant.url) {
                continue;
            }
            const bool preferred = !groups.isEmpty() && entry.group == groups.first();
            if (variant.bandwidth == 0 || preferred) {
                variant.bandwidth = entry.bandwidth;
            }
            if (preferred) {
                break;
            }
        }
    }
    std::stable_sort(master.variants.begin(), master.variants.end(), [](const HlsVariant &a, const HlsVariant &b) {
        if (a.resolution.height() != b.resolution.height()) {
            return a.resolution.height() > b.resolution.height();
        }
        const int rankA = videoRank(videoCodecOf(a.codecs));
        const int rankB = videoRank(videoCodecOf(b.codecs));
        if (rankA != rankB) {
            return (rankA < 0 ? 1 : rankA) < (rankB < 0 ? 1 : rankB);
        }
        return a.bandwidth > b.bandwidth;
    });

    QSet<QString> codecNames;
    for (const HlsVariant &variant : std::as_const(master.variants)) {
        codecNames.insert(videoCodecName(videoCodecOf(variant.codecs)));
    }
    QStringList codecSuffixes;
    QStringList bitrateSuffixes;
    QStringList ordinalSuffixes;
    int ordinal = 0;
    for (HlsVariant &variant : master.variants) {
        ++ordinal;
        const QString codecName = videoCodecName(videoCodecOf(variant.codecs));
        if (variant.resolution.height() > 0) {
            variant.label = QStringLiteral("%1p").arg(variant.resolution.height());
        } else if (variant.bandwidth > 0) {
            variant.label = bitrateText(variant.bandwidth);
        } else {
            variant.label = QStringLiteral("Stream %1").arg(ordinal);
        }
        codecSuffixes.append(codecNames.size() > 1 && !codecName.isEmpty() ? QLatin1Char(' ') + codecName : QString());
        bitrateSuffixes.append(variant.resolution.height() > 0 && variant.bandwidth > 0 ? QStringLiteral(" · ") + bitrateText(variant.bandwidth) : QString());
        ordinalSuffixes.append(QStringLiteral(" #%1").arg(ordinal));
    }
    disambiguate(master.variants, codecSuffixes);
    disambiguate(master.variants, bitrateSuffixes);
    disambiguate(master.variants, ordinalSuffixes);
    return master;
}

HlsPlayback playback(const HlsMaster &master, int variant, int audioFormat) {
    if (variant < 0 || variant >= master.variants.size()) {
        return {};
    }
    const QUrl url = master.variants[variant].url;
    const QString group = audioFormat >= 0 && audioFormat < master.audioFormats.size() ? master.audioFormats[audioFormat].group : QString();
    const HlsMaster::Entry *chosen = nullptr;
    for (const HlsMaster::Entry &entry : master.entries) {
        if (entry.url != url) {
            continue;
        }
        if (!chosen) {
            chosen = &entry;
        }
        if (entry.group == group) {
            chosen = &entry;
            break;
        }
    }
    if (!chosen) {
        return {};
    }
    if (!master.separateGroups.value(chosen->group, false)) {
        return {chosen->url, {}, chosen->bandwidth};
    }
    const QString manifest = QStringLiteral("#EXTM3U\n") + master.audioLines.value(chosen->group).join(QLatin1Char('\n')) + QLatin1Char('\n') + chosen->line +
                             QLatin1Char('\n') + QString::fromLatin1(chosen->url.toEncoded()) + QLatin1Char('\n');
    return {chosen->url, manifest.toUtf8(), chosen->bandwidth};
}

int mediumIndex(const QList<HlsVariant> &variants) { return variants.isEmpty() ? -1 : videoCount(variants) / 2; }

int indexForBitrate(const QList<HlsVariant> &variants, qint64 bitsPerSecond, int maxHeight) {
    if (variants.isEmpty()) {
        return -1;
    }
    if (bitsPerSecond <= 0 || std::none_of(variants.begin(), variants.end(), [](const HlsVariant &variant) { return variant.bandwidth > 0; })) {
        return mediumIndex(variants);
    }
    const int candidates = videoCount(variants);
    for (int i = 0; i < candidates; ++i) {
        const HlsVariant &variant = variants[i];
        if (maxHeight > 0 && variant.resolution.height() > maxHeight && i + 1 < candidates) {
            continue;
        }
        if (variant.bandwidth > 0 && variant.bandwidth * 3 / 2 <= bitsPerSecond) {
            return i;
        }
    }
    return candidates - 1;
}

QList<QUrl> segments(const QByteArray &mediaPlaylist, const QUrl &base) {
    QList<QUrl> result;
    const QStringList lines = QString::fromUtf8(mediaPlaylist).split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const QUrl url = base.resolved(QUrl(line));
        if (isHttpUrl(url) && (result.isEmpty() || result.last() != url)) {
            result.append(url);
        }
    }
    return result;
}

}
