#ifndef PROBEMESSAGE_H
#define PROBEMESSAGE_H

#include <QByteArray>
#include <QList>
#include <QString>

struct ProbeChapter {
    qint64 startMs = 0;
    qint64 endMs = 0;
    QString title;
};

struct ProbeSubtitleStream {
    int stream = -1;
    QString codec;
    QString language;
    QString title;
    bool isDefault = false;
    bool forced = false;
};

struct ProbeFont {
    QString name;
    QString path;
};

bool isRemoteSubtitleSource(const QString &path);

struct ProbeEvent {
    qint64 startMs = 0;
    qint64 durationMs = 0;
    QByteArray ass;
};

struct ProbeMessage {
    enum class Type { Invalid, Unknown, Hello, Media, Header, Event, End, Progress, Warning, Error };

    static constexpr int protocolVersion = 1;

    Type type = Type::Invalid;
    int protocol = 0;
    qint64 durationMs = 0;
    QList<ProbeChapter> chapters;
    QList<ProbeSubtitleStream> subtitleStreams;
    QList<ProbeFont> fonts;
    QString source;
    QString assHeader;
    QString encoding;
    ProbeEvent event;
    qint64 bytes = 0;
    QString code;
    QString detail;

    bool isValid() const { return type != Type::Invalid; }

    static ProbeMessage parse(const QByteArray &line);
    QByteArray serialise() const;

    static ProbeMessage hello();
    static ProbeMessage media(qint64 durationMs, const QList<ProbeChapter> &chapters, const QList<ProbeSubtitleStream> &streams,
                              const QList<ProbeFont> &fonts);
    static ProbeMessage header(const QString &source, const QString &assHeader, const QString &encoding = {});
    static ProbeMessage eventFor(const QString &source, const ProbeEvent &event);
    static ProbeMessage end(const QString &source);
    static ProbeMessage progress(qint64 bytes);
    static ProbeMessage warning(const QString &code, const QString &detail);
    static ProbeMessage error(const QString &code, const QString &detail);
};

#endif // PROBEMESSAGE_H
