#ifndef LOGOPROVIDER_H
#define LOGOPROVIDER_H

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QQuickAsyncImageProvider>
#include <QString>
#include <QThreadPool>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <atomic>

class QJSEngine;
class QNetworkAccessManager;
class QNetworkReply;
class QQmlEngine;

class LogoCache {
public:
    static constexpr qint64 defaultMaxBytes = 20 * 1024 * 1024;
    static constexpr int maxSourceSide = 1024;
    static constexpr int storedSide = 96;

    explicit LogoCache(const QString &directory, qint64 maxBytes = defaultMaxBytes);

    static bool accepts(const QUrl &address);
    static QString fileName(const QUrl &address);
    static QImage decode(const QByteArray &data);

    QImage lookup(const QUrl &address) const;
    bool store(const QUrl &address, const QImage &image);
    void clear();
    qint64 totalBytes() const;

private:
    QString m_directory;
    qint64 m_maxBytes;
    mutable QMutex m_mutex;
};

class LogoProvider : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static constexpr int transferTimeoutMs = 10000;
    static constexpr qint64 maxDownloadBytes = 512 * 1024;
    static constexpr int maxInFlight = 4;

    explicit LogoProvider(const QString &cacheDirectory, QObject *parent = nullptr);
    ~LogoProvider() override;

    static LogoProvider *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static QString defaultCacheDirectory();

    LogoCache *cache();
    int pendingCount() const;

    Q_INVOKABLE void clearCache();
    Q_INVOKABLE void cancelPending();
    void request(quint64 id, const QUrl &address);
    void cancel(quint64 id);

signals:
    void finished(quint64 id, const QImage &image);

private:
    struct Job {
        QList<quint64> waiters;
        QNetworkReply *reply = nullptr;
    };

    void startNext();
    void start(const QUrl &address);
    void complete(const QUrl &address, const QImage &image);

    static inline LogoProvider *s_instance = nullptr;

    LogoCache m_cache;
    QThreadPool m_workers;
    QNetworkAccessManager *m_network = nullptr;
    QHash<QUrl, Job> m_jobs;
    QHash<quint64, QUrl> m_requests;
    QList<QUrl> m_queue;
    int m_active = 0;
};

class LogoImageProvider : public QQuickAsyncImageProvider {
public:
    explicit LogoImageProvider(LogoProvider *provider);

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    LogoProvider *m_provider;
    std::atomic<quint64> m_nextId{1};
};

#endif // LOGOPROVIDER_H
