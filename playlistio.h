#ifndef PLAYLISTIO_H
#define PLAYLISTIO_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

struct PlaylistEntry {
    QUrl location;
    QString title;
    int durationSec = -1;
};

struct PlaylistDocument {
    QList<PlaylistEntry> entries;
    int currentIndex = -1;
};

struct PlaylistReadResult {
    bool ok = false;
    QString error;
    PlaylistDocument document;
    int skippedMissing = 0;
    int skippedUnsupported = 0;
};

namespace PlaylistIO {

PlaylistReadResult parse(const QByteArray &data, const QString &baseDirectory, const QStringList &supportedExtensions);
PlaylistReadResult read(const QString &path, const QStringList &supportedExtensions);
QByteArray serialise(const PlaylistDocument &document);
bool write(const QString &path, const PlaylistDocument &document, QString *error = nullptr);

}

class PlaylistFiles : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit PlaylistFiles(QObject *parent = nullptr);

    static QVariantMap toVariant(const PlaylistReadResult &result);
    static PlaylistDocument fromVariant(const QVariantList &items, int currentIndex);

    Q_INVOKABLE void load(const QUrl &url, const QStringList &supportedExtensions, const QString &tag);
    Q_INVOKABLE void save(const QUrl &url, const QVariantList &items, int currentIndex, const QString &tag);
    Q_INVOKABLE void remove(const QUrl &url);

signals:
    void loaded(const QString &tag, const QVariantMap &result);
    void saved(const QString &tag, bool ok);
};

#endif // PLAYLISTIO_H
