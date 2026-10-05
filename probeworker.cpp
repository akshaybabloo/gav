#include "probeworker.h"

#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringDecoder>

#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <utility>

#include <spdlog/spdlog.h>
#include <uchardet/uchardet.h>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
#include <libavutil/mem.h>
}

namespace {

constexpr qint64 progressIntervalMs = 2000;
constexpr qint64 fallbackEventDurationMs = 5000;
constexpr qint64 maxEncodingSampleBytes = 4 * 1024 * 1024;
const AVRational millisecondBase{1, 1000};

struct FormatDeleter {
    void operator()(AVFormatContext *context) const { avformat_close_input(&context); }
};
struct CodecDeleter {
    void operator()(AVCodecContext *context) const { avcodec_free_context(&context); }
};
struct PacketDeleter {
    void operator()(AVPacket *packet) const { av_packet_free(&packet); }
};

using FormatPtr = std::unique_ptr<AVFormatContext, FormatDeleter>;
using CodecPtr = std::unique_ptr<AVCodecContext, CodecDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;

QString errorString(int error) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(error, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

QString tag(const AVDictionary *dictionary, const char *name) {
    const AVDictionaryEntry *entry = av_dict_get(dictionary, name, nullptr, 0);
    return entry ? QString::fromUtf8(entry->value) : QString();
}

FormatPtr openFormat(const QString &path, QString *error, bool analyse = true) {
    AVFormatContext *raw = nullptr;
    const QByteArray encoded = path.toUtf8();
    int result = avformat_open_input(&raw, encoded.constData(), nullptr, nullptr);
    if (result < 0) {
        *error = errorString(result);
        return {};
    }
    FormatPtr format(raw);
    if (!analyse) {
        return format;
    }
    result = avformat_find_stream_info(format.get(), nullptr);
    if (result < 0) {
        *error = errorString(result);
        return {};
    }
    return format;
}

bool isTextSubtitle(const AVStream *stream) {
    if (stream->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
        return false;
    }
    const AVCodecDescriptor *descriptor = avcodec_descriptor_get(stream->codecpar->codec_id);
    return descriptor && (descriptor->props & AV_CODEC_PROP_TEXT_SUB);
}

bool isFontAttachment(const AVStream *stream, const QString &name) {
    if (stream->codecpar->codec_type != AVMEDIA_TYPE_ATTACHMENT || !stream->codecpar->extradata || stream->codecpar->extradata_size <= 0) {
        return false;
    }
    const QString mimetype = tag(stream->metadata, "mimetype").toLower();
    const QString suffix = QFileInfo(name).suffix().toLower();
    return mimetype.contains(QLatin1String("font")) || suffix == QLatin1String("ttf") || suffix == QLatin1String("otf") ||
           suffix == QLatin1String("ttc") || stream->codecpar->codec_id == AV_CODEC_ID_TTF || stream->codecpar->codec_id == AV_CODEC_ID_OTF;
}

CodecPtr openDecoder(const AVStream *stream, const QByteArray &charset, QString *error) {
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        *error = QStringLiteral("No decoder for codec %1").arg(QString::fromUtf8(avcodec_get_name(stream->codecpar->codec_id)));
        return {};
    }
    CodecPtr context(avcodec_alloc_context3(codec));
    if (!context) {
        *error = QStringLiteral("Out of memory");
        return {};
    }
    int result = avcodec_parameters_to_context(context.get(), stream->codecpar);
    if (result < 0) {
        *error = errorString(result);
        return {};
    }
    context->pkt_timebase = stream->time_base;
    if (!charset.isEmpty()) {
        context->sub_charenc = av_strdup(charset.constData());
    }
    result = avcodec_open2(context.get(), codec, nullptr);
    if (result < 0) {
        *error = errorString(result);
        return {};
    }
    return context;
}

QString headerOf(const AVCodecContext *context) {
    if (!context->subtitle_header || context->subtitle_header_size <= 0) {
        return {};
    }
    return QString::fromUtf8(reinterpret_cast<const char *>(context->subtitle_header), context->subtitle_header_size);
}

qint64 packetTimeUs(const AVPacket *packet, const AVStream *stream) {
    const int64_t pts = packet->pts != AV_NOPTS_VALUE ? packet->pts : packet->dts;
    if (pts == AV_NOPTS_VALUE) {
        return AV_NOPTS_VALUE;
    }
    return av_rescale_q(pts, stream->time_base, AV_TIME_BASE_Q);
}

QList<ProbeEvent> decodePacket(AVCodecContext *context, const AVStream *stream, AVPacket *packet, qint64 startOffsetUs) {
    QList<ProbeEvent> events;
    AVSubtitle subtitle{};
    int gotSubtitle = 0;
    const int result = avcodec_decode_subtitle2(context, &subtitle, &gotSubtitle, packet);
    if (result < 0) {
        spdlog::debug("Subtitle decode failed in stream {}: {}", stream->index, errorString(result).toStdString());
        return events;
    }
    if (!gotSubtitle) {
        return events;
    }

    qint64 baseUs = subtitle.pts != AV_NOPTS_VALUE ? subtitle.pts : (packet ? packetTimeUs(packet, stream) : AV_NOPTS_VALUE);
    if (baseUs == AV_NOPTS_VALUE) {
        avsubtitle_free(&subtitle);
        return events;
    }

    const qint64 startMs = qMax<qint64>(0, (baseUs - startOffsetUs) / 1000 + subtitle.start_display_time);
    qint64 durationMs = 0;
    if (subtitle.end_display_time != UINT32_MAX && subtitle.end_display_time > subtitle.start_display_time) {
        durationMs = subtitle.end_display_time - subtitle.start_display_time;
    } else if (packet && packet->duration > 0) {
        durationMs = av_rescale_q(packet->duration, stream->time_base, millisecondBase);
    }
    if (durationMs <= 0) {
        durationMs = fallbackEventDurationMs;
    }

    for (unsigned i = 0; i < subtitle.num_rects; ++i) {
        const AVSubtitleRect *rect = subtitle.rects[i];
        if (rect && rect->ass && *rect->ass) {
            events.append({startMs, durationMs, QByteArray(rect->ass)});
        }
    }
    avsubtitle_free(&subtitle);
    return events;
}

QList<ProbeEvent> flushDecoder(AVCodecContext *context, const AVStream *stream, qint64 startOffsetUs) {
    QList<ProbeEvent> events;
    PacketPtr empty(av_packet_alloc());
    for (int i = 0; i < 64; ++i) {
        const QList<ProbeEvent> batch = decodePacket(context, stream, empty.get(), startOffsetUs);
        if (batch.isEmpty()) {
            break;
        }
        events.append(batch);
    }
    return events;
}

QString safeFontName(const QString &name, int index, const QDir &directory) {
    QString file = QFileInfo(name).fileName();
    file.remove(QLatin1Char('/'));
    file.remove(QLatin1Char('\\'));
    if (file.isEmpty() || file.startsWith(QLatin1Char('.'))) {
        file = QStringLiteral("font%1.ttf").arg(index);
    }
    if (directory.exists(file)) {
        file = QStringLiteral("%1-%2").arg(index).arg(file);
    }
    return file;
}

}

struct ProbeWorker::MediaContext {
    FormatPtr format;
    qint64 startOffsetUs = 0;
    QList<int> textStreams;
};

ProbeWorker::ProbeWorker(Writer writer) : m_writer(std::move(writer)) {}

QString ProbeWorker::detectEncoding(const QByteArray &data) {
    if (data.startsWith("\xEF\xBB\xBF") || data.startsWith("\xFF\xFE") || data.startsWith("\xFE\xFF")) {
        return {};
    }
    QStringDecoder utf8(QStringDecoder::Utf8);
    [[maybe_unused]] const QString decoded = utf8.decode(data);
    if (!utf8.hasError()) {
        return {};
    }

    uchardet_t detector = uchardet_new();
    if (!detector) {
        return {};
    }
    QString charset;
    if (uchardet_handle_data(detector, data.constData(), static_cast<size_t>(data.size())) == 0) {
        uchardet_data_end(detector);
        charset = QString::fromLatin1(uchardet_get_charset(detector)).toUpper();
    }
    uchardet_delete(detector);
    if (charset.isEmpty() || charset == QLatin1String("ASCII") || charset == QLatin1String("UTF-8")) {
        return QStringLiteral("WINDOWS-1252");
    }
    return charset;
}

void ProbeWorker::emitProgress(qint64 bytes, bool force) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (force || m_lastProgressMs < 0 || now - m_lastProgressMs >= progressIntervalMs) {
        m_lastProgressMs = now;
        m_writer(ProbeMessage::progress(qMax<qint64>(0, bytes)));
    }
}

int ProbeWorker::run(const ProbeOptions &options) {
    m_writer(ProbeMessage::hello());
    m_lastProgressMs = QDateTime::currentMSecsSinceEpoch();

    MediaContext context;
    QString openError;
    const bool mediaOpened = options.mediaPath.isEmpty() || probeMedia(options, context, &openError);

    for (const QString &path : options.subtitleFiles) {
        probeExternal(path);
    }

    if (!mediaOpened) {
        m_writer(ProbeMessage::error(QStringLiteral("open-failed"), openError));
        return 1;
    }
    if (context.format && !streamEmbeddedEvents(options, context)) {
        return 1;
    }
    return 0;
}

bool ProbeWorker::probeMedia(const ProbeOptions &options, MediaContext &context, QString *error) {
    context.format = openFormat(options.mediaPath, error);
    if (!context.format) {
        return false;
    }

    AVFormatContext *format = context.format.get();
    context.startOffsetUs = format->start_time != AV_NOPTS_VALUE ? format->start_time : 0;
    const qint64 durationMs = format->duration != AV_NOPTS_VALUE && format->duration > 0 ? format->duration / 1000 : 0;

    QList<ProbeChapter> chapters;
    for (unsigned i = 0; i < format->nb_chapters; ++i) {
        const AVChapter *chapter = format->chapters[i];
        const qint64 offsetMs = context.startOffsetUs / 1000;
        const qint64 start = qMax<qint64>(0, av_rescale_q(chapter->start, chapter->time_base, millisecondBase) - offsetMs);
        const qint64 end = qMax<qint64>(start, av_rescale_q(chapter->end, chapter->time_base, millisecondBase) - offsetMs);
        chapters.append({start, end, tag(chapter->metadata, "title")});
    }
    std::stable_sort(chapters.begin(), chapters.end(), [](const ProbeChapter &a, const ProbeChapter &b) { return a.startMs < b.startMs; });
    QList<ProbeChapter> cleaned;
    for (const ProbeChapter &chapter : chapters) {
        if (!cleaned.isEmpty() && chapter.startMs <= cleaned.last().startMs) {
            continue;
        }
        if (!cleaned.isEmpty() && cleaned.last().endMs > chapter.startMs) {
            cleaned.last().endMs = chapter.startMs;
        }
        cleaned.append(chapter);
    }
    for (qsizetype i = 0; i < cleaned.size(); ++i) {
        if (cleaned[i].title.isEmpty()) {
            cleaned[i].title = QStringLiteral("Chapter %1").arg(i + 1);
        }
    }

    QList<ProbeSubtitleStream> streams;
    QList<ProbeFont> fonts;
    const QDir fontsDir(options.fontsDir);
    for (unsigned i = 0; i < format->nb_streams; ++i) {
        const AVStream *stream = format->streams[i];
        if (isTextSubtitle(stream)) {
            context.textStreams.append(stream->index);
            streams.append({stream->index, QString::fromUtf8(avcodec_get_name(stream->codecpar->codec_id)), tag(stream->metadata, "language"),
                            tag(stream->metadata, "title"), (stream->disposition & AV_DISPOSITION_DEFAULT) != 0,
                            (stream->disposition & AV_DISPOSITION_FORCED) != 0});
            continue;
        }

        const QString name = tag(stream->metadata, "filename");
        if (options.fontsDir.isEmpty() || !isFontAttachment(stream, name)) {
            continue;
        }
        const QString fileName = safeFontName(name, int(i), fontsDir);
        QFile file(fontsDir.filePath(fileName));
        if (file.open(QIODevice::WriteOnly) &&
            file.write(reinterpret_cast<const char *>(stream->codecpar->extradata), stream->codecpar->extradata_size) ==
                stream->codecpar->extradata_size) {
            fonts.append({name.isEmpty() ? fileName : name, QFileInfo(file).absoluteFilePath()});
        } else {
            m_writer(ProbeMessage::warning(QStringLiteral("font-write-failed"), name.isEmpty() ? fileName : name));
        }
    }

    m_writer(ProbeMessage::media(durationMs, cleaned, streams, fonts));
    return true;
}

void ProbeWorker::probeExternal(const QString &path) {
    const bool remote = isRemoteSubtitleSource(path);
    const QString absolute = remote ? path : QFileInfo(path).absoluteFilePath();
    const QString source = QStringLiteral("external:") + absolute;
    const auto fail = [&](const QString &detail) {
        m_writer(ProbeMessage::warning(QStringLiteral("source-failed"), source + QLatin1Char('\t') + detail));
    };

    QByteArray charset;
    if (!remote) {
        QFile file(absolute);
        if (!file.open(QIODevice::ReadOnly)) {
            fail(file.errorString());
            return;
        }
        charset = detectEncoding(file.read(maxEncodingSampleBytes)).toLatin1();
    }

    QString error;
    FormatPtr format = openFormat(absolute, &error, !remote);
    if (!format) {
        fail(error);
        return;
    }

    const AVStream *stream = nullptr;
    for (unsigned i = 0; i < format->nb_streams; ++i) {
        if (isTextSubtitle(format->streams[i])) {
            stream = format->streams[i];
            break;
        }
    }
    if (!stream) {
        fail(QStringLiteral("No text subtitle stream"));
        return;
    }

    CodecPtr decoder = openDecoder(stream, charset, &error);
    if (!decoder && !charset.isEmpty()) {
        m_writer(ProbeMessage::warning(QStringLiteral("encoding-failed"), source + QLatin1Char('\t') + QString::fromLatin1(charset)));
        charset.clear();
        decoder = openDecoder(stream, charset, &error);
    }
    if (!decoder) {
        fail(error);
        return;
    }

    const qint64 startOffsetUs = !remote && format->start_time != AV_NOPTS_VALUE && format->start_time > 0 ? format->start_time : 0;
    m_writer(ProbeMessage::header(source, headerOf(decoder.get()), QString::fromLatin1(charset)));

    PacketPtr packet(av_packet_alloc());
    while (av_read_frame(format.get(), packet.get()) >= 0) {
        if (packet->stream_index == stream->index) {
            for (const ProbeEvent &event : decodePacket(decoder.get(), stream, packet.get(), startOffsetUs)) {
                m_writer(ProbeMessage::eventFor(source, event));
            }
        }
        av_packet_unref(packet.get());
        emitProgress(format->pb ? avio_tell(format->pb) : 0);
    }
    for (const ProbeEvent &event : flushDecoder(decoder.get(), stream, startOffsetUs)) {
        m_writer(ProbeMessage::eventFor(source, event));
    }
    m_writer(ProbeMessage::end(source));
}

bool ProbeWorker::streamEmbeddedEvents(const ProbeOptions &options, MediaContext &context) {
    if (options.events == QLatin1String("none") || context.textStreams.isEmpty()) {
        return true;
    }

    QList<int> selected = context.textStreams;
    if (options.events != QLatin1String("all")) {
        bool ok = false;
        const int index = options.events.toInt(&ok);
        selected = ok && context.textStreams.contains(index) ? QList<int>{index} : QList<int>{};
    }

    AVFormatContext *format = context.format.get();
    std::map<int, CodecPtr> decoders;
    for (const int index : std::as_const(selected)) {
        QString error;
        CodecPtr decoder = openDecoder(format->streams[index], {}, &error);
        const QString source = QStringLiteral("embedded:%1").arg(index);
        if (!decoder) {
            m_writer(ProbeMessage::warning(QStringLiteral("source-failed"), source + QLatin1Char('\t') + error));
            continue;
        }
        m_writer(ProbeMessage::header(source, headerOf(decoder.get())));
        decoders.emplace(index, std::move(decoder));
    }
    if (decoders.empty()) {
        return true;
    }

    for (unsigned i = 0; i < format->nb_streams; ++i) {
        format->streams[i]->discard = decoders.count(int(i)) ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
    }

    PacketPtr packet(av_packet_alloc());
    int result = 0;
    while ((result = av_read_frame(format, packet.get())) >= 0) {
        const auto it = decoders.find(packet->stream_index);
        if (it != decoders.end()) {
            const AVStream *stream = format->streams[packet->stream_index];
            const QString source = QStringLiteral("embedded:%1").arg(packet->stream_index);
            for (const ProbeEvent &event : decodePacket(it->second.get(), stream, packet.get(), context.startOffsetUs)) {
                m_writer(ProbeMessage::eventFor(source, event));
            }
        }
        av_packet_unref(packet.get());
        emitProgress(format->pb ? avio_tell(format->pb) : 0);
    }

    if (result != AVERROR_EOF) {
        m_writer(ProbeMessage::error(QStringLiteral("decode-failed"), errorString(result)));
        return false;
    }

    for (const auto &[index, decoder] : decoders) {
        const AVStream *stream = format->streams[index];
        const QString source = QStringLiteral("embedded:%1").arg(index);
        for (const ProbeEvent &event : flushDecoder(decoder.get(), stream, context.startOffsetUs)) {
            m_writer(ProbeMessage::eventFor(source, event));
        }
        m_writer(ProbeMessage::end(source));
    }
    return true;
}

int ProbeWorker::runFromArguments(const QStringList &arguments) {
#ifdef Q_OS_WIN
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    av_log_set_level(AV_LOG_ERROR);

    QCommandLineParser parser;
    const QCommandLineOption probeOption(QStringLiteral("probe"), QString(), QStringLiteral("path"));
    const QCommandLineOption fontsOption(QStringLiteral("fonts-dir"), QString(), QStringLiteral("dir"));
    const QCommandLineOption subtitleOption(QStringLiteral("subtitle-file"), QString(), QStringLiteral("path"));
    const QCommandLineOption eventsOption(QStringLiteral("events"), QString(), QStringLiteral("which"), QStringLiteral("all"));
    parser.addOptions({probeOption, fontsOption, subtitleOption, eventsOption});

    const auto write = [](const ProbeMessage &message) {
        const QByteArray line = message.serialise() + '\n';
        std::fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stdout);
        std::fflush(stdout);
    };

    if (!parser.parse(arguments)) {
        write(ProbeMessage::hello());
        write(ProbeMessage::error(QStringLiteral("internal"), parser.errorText()));
        return 1;
    }

    ProbeOptions options;
    options.mediaPath = parser.value(probeOption);
    options.fontsDir = parser.value(fontsOption);
    options.subtitleFiles = parser.values(subtitleOption);
    options.events = parser.value(eventsOption);
    if (options.mediaPath.isEmpty() && options.subtitleFiles.isEmpty()) {
        write(ProbeMessage::hello());
        write(ProbeMessage::error(QStringLiteral("no-streams"), QStringLiteral("Nothing to probe")));
        return 1;
    }

    ProbeWorker worker(write);
    return worker.run(options);
}
