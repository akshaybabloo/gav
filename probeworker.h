#ifndef PROBEWORKER_H
#define PROBEWORKER_H

#include "probemessage.h"

#include <QString>
#include <QStringList>

#include <functional>

struct ProbeOptions {
    QString mediaPath;
    QString fontsDir;
    QStringList subtitleFiles;
    QString events = QStringLiteral("all");
};

class ProbeWorker {
public:
    using Writer = std::function<void(const ProbeMessage &)>;

    explicit ProbeWorker(Writer writer);

    int run(const ProbeOptions &options);

    static int runFromArguments(const QStringList &arguments);
    static QString detectEncoding(const QByteArray &data);

private:
    struct MediaContext;

    bool probeMedia(const ProbeOptions &options, MediaContext &context, QString *error);
    void probeExternal(const QString &path);
    bool streamEmbeddedEvents(const ProbeOptions &options, MediaContext &context);
    void emitProgress(qint64 bytes, bool force = false);

    Writer m_writer;
    qint64 m_lastProgressMs = -1;
};

#endif // PROBEWORKER_H
