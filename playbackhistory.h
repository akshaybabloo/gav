#ifndef PLAYBACKHISTORY_H
#define PLAYBACKHISTORY_H

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QtQml/qqmlregistration.h>

class PlaybackHistory : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static constexpr int maxPositions = 200;
    static constexpr int maxRecent = 10;
    static constexpr int formatVersion = 1;

    struct PositionEntry {
        QString path;
        qint64 positionMs = 0;
        qint64 durationMs = 0;
        QDateTime lastPlayed;
    };

    struct RecentEntry {
        QString path;
        QDateTime lastOpened;
    };

    explicit PlaybackHistory(QObject *parent = nullptr);
    explicit PlaybackHistory(const QString &storagePath, QObject *parent = nullptr);
    ~PlaybackHistory() override;

    static QString defaultStoragePath();
    static QString normalizePath(const QString &pathOrUrl);
    static bool isNetworkUrl(const QString &pathOrUrl);

    QString storagePath() const;
    void waitForPendingWrites();

    Q_INVOKABLE qint64 savedPosition(const QString &path) const;
    Q_INVOKABLE void recordPosition(const QString &path, qint64 positionMs, qint64 durationMs);
    Q_INVOKABLE void removePosition(const QString &path);
    Q_INVOKABLE void recordOpened(const QString &pathOrUrl);
    Q_INVOKABLE QStringList recentFiles() const;
    Q_INVOKABLE void removeRecent(const QString &pathOrUrl);
    Q_INVOKABLE void clearRecent();
    Q_INVOKABLE void clear();

signals:
    void recentChanged();

private:
    void load();
    void save();

    QString m_storagePath;
    QList<PositionEntry> m_positions;
    QList<RecentEntry> m_recent;
    QThreadPool m_writer;
};

#endif // PLAYBACKHISTORY_H
