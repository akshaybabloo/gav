#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class PlaylistModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int currentRow READ currentRow WRITE setCurrentRow NOTIFY currentRowChanged)
    Q_PROPERTY(int currentId READ currentId NOTIFY currentChanged)
    Q_PROPERTY(QStringList audioExtensions READ audioExtensions WRITE setAudioExtensions NOTIFY audioExtensionsChanged)

public:
    enum Kind { LocalVideo, LocalAudio, Stream };
    Q_ENUM(Kind)

    enum StreamState { StreamUnknown, StreamOnDemand, StreamLive };
    Q_ENUM(StreamState)

    enum Availability { AvailabilityUnknown, Playable, Unavailable };
    Q_ENUM(Availability)

    enum Role {
        EntryIdRole = Qt::UserRole + 1,
        LocationRole,
        TitleRole,
        KindRole,
        StreamStateRole,
        GroupRole,
        LogoRole,
        DurationMsRole,
        AvailableRole,
        ReasonRole,
        IsCurrentRole,
        QueuePositionRole,
    };

    struct Entry {
        int id = -1;
        QUrl location;
        QString title;
        Kind kind = LocalVideo;
        StreamState streamState = StreamUnknown;
        QString group;
        QUrl logo;
        qint64 durationMs = -1;
        Availability availability = AvailabilityUnknown;
        QString reason;
        QString attributes;
    };

    explicit PlaylistModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    int currentRow() const;
    void setCurrentRow(int row);
    int currentId() const;
    int anchorRow() const;
    QStringList audioExtensions() const;
    void setAudioExtensions(const QStringList &extensions);

    const Entry *entry(int row) const;

    Q_INVOKABLE int append(const QVariantList &entries);
    Q_INVOKABLE int insert(int row, const QVariantList &entries);
    Q_INVOKABLE int remove(const QList<int> &ids);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setLoaded(int id, qint64 durationMs, bool isLive);
    Q_INVOKABLE void setUnavailable(int id, const QString &reason);
    Q_INVOKABLE int rowForId(int id) const;
    Q_INVOKABLE int idAt(int row) const;
    Q_INVOKABLE QVariantMap entryAt(int row) const;
    Q_INVOKABLE QVariantList toVariantList() const;
    Q_INVOKABLE QList<QUrl> locations() const;

signals:
    void countChanged();
    void currentChanged();
    void currentRowChanged();
    void audioExtensionsChanged();

private:
    bool makeEntry(const QVariantMap &map, Entry *entry);
    QVariantMap toMap(const Entry &entry) const;
    void setCurrentId(int id);

    QList<Entry> m_entries;
    QStringList m_audioExtensions;
    int m_nextId = 1;
    int m_currentId = -1;
    int m_anchorRow = -1;
};

#endif // PLAYLISTMODEL_H
