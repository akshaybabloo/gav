#include "probemessage.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

QString key(const char *name) { return QString::fromLatin1(name); }

bool readString(const QJsonObject &object, const char *name, QString *out, bool required = true) {
    const QJsonValue value = object.value(key(name));
    if (value.isUndefined() || value.isNull()) {
        return !required;
    }
    if (!value.isString()) {
        return false;
    }
    *out = value.toString();
    return !required || !out->isEmpty();
}

bool readInteger(const QJsonObject &object, const char *name, qint64 *out, bool required = true) {
    const QJsonValue value = object.value(key(name));
    if (value.isUndefined()) {
        return !required;
    }
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (number < 0 || number > 9.0e15 || number != static_cast<double>(static_cast<qint64>(number))) {
        return false;
    }
    *out = static_cast<qint64>(number);
    return true;
}

bool readBool(const QJsonObject &object, const char *name, bool *out) {
    const QJsonValue value = object.value(key(name));
    if (value.isUndefined()) {
        return true;
    }
    if (!value.isBool()) {
        return false;
    }
    *out = value.toBool();
    return true;
}

bool parseMedia(const QJsonObject &object, ProbeMessage *message) {
    if (!readInteger(object, "durationMs", &message->durationMs, false)) {
        return false;
    }
    for (const QJsonValue &value : object.value(key("chapters")).toArray()) {
        const QJsonObject chapter = value.toObject();
        ProbeChapter parsed;
        if (!readInteger(chapter, "startMs", &parsed.startMs) || !readInteger(chapter, "endMs", &parsed.endMs) ||
            !readString(chapter, "title", &parsed.title, false) || parsed.endMs < parsed.startMs) {
            return false;
        }
        message->chapters.append(parsed);
    }
    for (const QJsonValue &value : object.value(key("subtitleStreams")).toArray()) {
        const QJsonObject stream = value.toObject();
        ProbeSubtitleStream parsed;
        qint64 index = -1;
        if (!readInteger(stream, "stream", &index) || index > 1'000'000 || !readString(stream, "codec", &parsed.codec, false) ||
            !readString(stream, "language", &parsed.language, false) || !readString(stream, "title", &parsed.title, false) ||
            !readBool(stream, "default", &parsed.isDefault) || !readBool(stream, "forced", &parsed.forced)) {
            return false;
        }
        parsed.stream = static_cast<int>(index);
        message->subtitleStreams.append(parsed);
    }
    for (const QJsonValue &value : object.value(key("fonts")).toArray()) {
        const QJsonObject font = value.toObject();
        ProbeFont parsed;
        if (!readString(font, "name", &parsed.name) || !readString(font, "path", &parsed.path)) {
            return false;
        }
        message->fonts.append(parsed);
    }
    return true;
}

QJsonObject typed(const char *type) { return QJsonObject{{key("type"), key(type)}}; }

}

ProbeMessage ProbeMessage::parse(const QByteArray &line) {
    ProbeMessage message;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(line, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return message;
    }

    const QJsonObject object = document.object();
    const QString type = object.value(key("type")).toString();
    bool ok = false;

    if (type == QLatin1String("hello")) {
        qint64 protocol = 0;
        ok = readInteger(object, "protocol", &protocol) && protocol <= 1000;
        message.protocol = static_cast<int>(protocol);
        message.type = Type::Hello;
    } else if (type == QLatin1String("media")) {
        ok = parseMedia(object, &message);
        message.type = Type::Media;
    } else if (type == QLatin1String("header")) {
        ok = readString(object, "source", &message.source) && readString(object, "assHeader", &message.assHeader, false) &&
             readString(object, "encoding", &message.encoding, false);
        message.type = Type::Header;
    } else if (type == QLatin1String("event")) {
        QString ass;
        ok = readString(object, "source", &message.source) && readInteger(object, "startMs", &message.event.startMs) &&
             readInteger(object, "durationMs", &message.event.durationMs) && readString(object, "ass", &ass, false);
        message.event.ass = ass.toUtf8();
        message.type = Type::Event;
    } else if (type == QLatin1String("end")) {
        ok = readString(object, "source", &message.source);
        message.type = Type::End;
    } else if (type == QLatin1String("progress")) {
        ok = readInteger(object, "bytes", &message.bytes);
        message.type = Type::Progress;
    } else if (type == QLatin1String("warning") || type == QLatin1String("error")) {
        ok = readString(object, "code", &message.code) && readString(object, "detail", &message.detail, false);
        message.type = type == QLatin1String("warning") ? Type::Warning : Type::Error;
    } else {
        ok = !type.isEmpty();
        message.type = Type::Unknown;
    }

    if (!ok) {
        return {};
    }
    return message;
}

QByteArray ProbeMessage::serialise() const {
    QJsonObject object;
    switch (type) {
    case Type::Invalid:
    case Type::Unknown:
        return {};
    case Type::Hello:
        object = typed("hello");
        object.insert(key("protocol"), protocol);
        break;
    case Type::Media: {
        object = typed("media");
        object.insert(key("durationMs"), durationMs);
        QJsonArray chapterArray;
        for (const ProbeChapter &chapter : chapters) {
            chapterArray.append(QJsonObject{{key("startMs"), chapter.startMs}, {key("endMs"), chapter.endMs}, {key("title"), chapter.title}});
        }
        QJsonArray streamArray;
        for (const ProbeSubtitleStream &stream : subtitleStreams) {
            streamArray.append(QJsonObject{{key("stream"), stream.stream},
                                           {key("codec"), stream.codec},
                                           {key("language"), stream.language},
                                           {key("title"), stream.title},
                                           {key("default"), stream.isDefault},
                                           {key("forced"), stream.forced}});
        }
        QJsonArray fontArray;
        for (const ProbeFont &font : fonts) {
            fontArray.append(QJsonObject{{key("name"), font.name}, {key("path"), font.path}});
        }
        object.insert(key("chapters"), chapterArray);
        object.insert(key("subtitleStreams"), streamArray);
        object.insert(key("fonts"), fontArray);
        break;
    }
    case Type::Header:
        object = typed("header");
        object.insert(key("source"), source);
        object.insert(key("assHeader"), assHeader);
        if (!encoding.isEmpty()) {
            object.insert(key("encoding"), encoding);
        }
        break;
    case Type::Event:
        object = typed("event");
        object.insert(key("source"), source);
        object.insert(key("startMs"), event.startMs);
        object.insert(key("durationMs"), event.durationMs);
        object.insert(key("ass"), QString::fromUtf8(event.ass));
        break;
    case Type::End:
        object = typed("end");
        object.insert(key("source"), source);
        break;
    case Type::Progress:
        object = typed("progress");
        object.insert(key("bytes"), bytes);
        break;
    case Type::Warning:
    case Type::Error:
        object = typed(type == Type::Warning ? "warning" : "error");
        object.insert(key("code"), code);
        object.insert(key("detail"), detail);
        break;
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

ProbeMessage ProbeMessage::hello() {
    ProbeMessage message;
    message.type = Type::Hello;
    message.protocol = protocolVersion;
    return message;
}

ProbeMessage ProbeMessage::media(qint64 durationMs, const QList<ProbeChapter> &chapters, const QList<ProbeSubtitleStream> &streams,
                                 const QList<ProbeFont> &fonts) {
    ProbeMessage message;
    message.type = Type::Media;
    message.durationMs = durationMs;
    message.chapters = chapters;
    message.subtitleStreams = streams;
    message.fonts = fonts;
    return message;
}

ProbeMessage ProbeMessage::header(const QString &source, const QString &assHeader, const QString &encoding) {
    ProbeMessage message;
    message.type = Type::Header;
    message.source = source;
    message.assHeader = assHeader;
    message.encoding = encoding;
    return message;
}

ProbeMessage ProbeMessage::eventFor(const QString &source, const ProbeEvent &event) {
    ProbeMessage message;
    message.type = Type::Event;
    message.source = source;
    message.event = event;
    return message;
}

ProbeMessage ProbeMessage::end(const QString &source) {
    ProbeMessage message;
    message.type = Type::End;
    message.source = source;
    return message;
}

ProbeMessage ProbeMessage::progress(qint64 bytes) {
    ProbeMessage message;
    message.type = Type::Progress;
    message.bytes = bytes;
    return message;
}

ProbeMessage ProbeMessage::warning(const QString &code, const QString &detail) {
    ProbeMessage message;
    message.type = Type::Warning;
    message.code = code;
    message.detail = detail;
    return message;
}

ProbeMessage ProbeMessage::error(const QString &code, const QString &detail) {
    ProbeMessage message;
    message.type = Type::Error;
    message.code = code;
    message.detail = detail;
    return message;
}
