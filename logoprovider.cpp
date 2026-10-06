#include "logoprovider.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QJSEngine>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include <cstring>

namespace {

QImage withoutMetadata(const QImage &image) {
    const QImage source = image.convertToFormat(QImage::Format_ARGB32);
    QImage copy(source.size(), QImage::Format_ARGB32);
    if (copy.isNull()) {
        return {};
    }
    const qsizetype rowBytes = qsizetype(copy.width()) * 4;
    for (int y = 0; y < copy.height(); ++y) {
        std::memcpy(copy.scanLine(y), source.constScanLine(y), size_t(rowBytes));
    }
    return copy;
}

QImage placeholder() {
    QImage image(1, 1, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    return image;
}

class LogoResponse : public QQuickImageResponse {
public:
    LogoResponse(LogoProvider *provider, quint64 id, const QUrl &address) : m_provider(provider), m_id(id) {
        if (!LogoCache::accepts(address)) {
            QMetaObject::invokeMethod(this, [this] { finish(QImage()); }, Qt::QueuedConnection);
            return;
        }
        const QImage cached = provider->cache()->lookup(address);
        if (!cached.isNull()) {
            QMetaObject::invokeMethod(this, [this, cached] { finish(cached); }, Qt::QueuedConnection);
            return;
        }
        connect(provider, &LogoProvider::finished, this, [this](quint64 id, const QImage &image) {
            if (id == m_id) {
                finish(image);
            }
        });
        m_requested = true;
        QMetaObject::invokeMethod(provider, [provider, id, address] { provider->request(id, address); }, Qt::QueuedConnection);
    }

    QQuickTextureFactory *textureFactory() const override { return QQuickTextureFactory::textureFactoryForImage(m_image); }

    void cancel() override {
        if (m_requested && !m_finished) {
            LogoProvider *provider = m_provider;
            const quint64 id = m_id;
            QMetaObject::invokeMethod(provider, [provider, id] { provider->cancel(id); }, Qt::QueuedConnection);
        }
    }

private:
    void finish(const QImage &image) {
        if (m_finished) {
            return;
        }
        m_finished = true;
        m_image = image.isNull() ? placeholder() : image;
        emit finished();
    }

    LogoProvider *m_provider;
    quint64 m_id;
    QImage m_image;
    bool m_requested = false;
    bool m_finished = false;
};

}

LogoCache::LogoCache(const QString &directory, qint64 maxBytes) : m_directory(directory), m_maxBytes(maxBytes) {}

bool LogoCache::accepts(const QUrl &address) {
    const QString scheme = address.scheme().toLower();
    return address.isValid() && !address.host().isEmpty() && (scheme == QLatin1String("http") || scheme == QLatin1String("https"));
}

QString LogoCache::fileName(const QUrl &address) {
    if (!accepts(address)) {
        return {};
    }
    const QByteArray hash = QCryptographicHash::hash(address.toString(QUrl::FullyEncoded).toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex()) + QStringLiteral(".png");
}

QImage LogoCache::decode(const QByteArray &data) {
    if (data.isEmpty()) {
        return {};
    }
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setDecideFormatFromContent(true);
    static const QList<QByteArray> allowed{"png", "jpeg", "jpg", "webp"};
    if (!allowed.contains(reader.format().toLower())) {
        return {};
    }
    const QSize size = reader.size();
    if (size.isEmpty() || size.width() > maxSourceSide || size.height() > maxSourceSide) {
        return {};
    }
    if (size.width() > storedSide || size.height() > storedSide) {
        reader.setScaledSize(size.scaled(storedSide, storedSide, Qt::KeepAspectRatio).expandedTo(QSize(1, 1)));
    }
    const QImage image = reader.read();
    return image.isNull() ? QImage() : withoutMetadata(image);
}

QImage LogoCache::lookup(const QUrl &address) const {
    const QString name = fileName(address);
    if (name.isEmpty()) {
        return {};
    }
    QMutexLocker locker(&m_mutex);
    const QString path = QDir(m_directory).filePath(name);
    if (!QFileInfo::exists(path)) {
        return {};
    }
    QImageReader reader(path, "png");
    return reader.read();
}

bool LogoCache::store(const QUrl &address, const QImage &image) {
    const QString name = fileName(address);
    if (name.isEmpty() || image.isNull()) {
        return false;
    }
    const QImage plain = withoutMetadata(image);
    QMutexLocker locker(&m_mutex);
    QDir directory(m_directory);
    if (!directory.mkpath(QStringLiteral("."))) {
        return false;
    }
    QSaveFile file(directory.filePath(name));
    if (!file.open(QIODevice::WriteOnly) || !plain.save(&file, "PNG") || !file.commit()) {
        return false;
    }

    QFileInfoList files = directory.entryInfoList(QDir::Files, QDir::Time | QDir::Reversed);
    qint64 total = 0;
    for (const QFileInfo &info : files) {
        total += info.size();
    }
    for (const QFileInfo &info : files) {
        if (total <= m_maxBytes) {
            break;
        }
        if (info.fileName() != name && QFile::remove(info.absoluteFilePath())) {
            total -= info.size();
        }
    }
    return true;
}

void LogoCache::clear() {
    QMutexLocker locker(&m_mutex);
    const QFileInfoList files = QDir(m_directory).entryInfoList(QDir::Files | QDir::Hidden);
    for (const QFileInfo &info : files) {
        QFile::remove(info.absoluteFilePath());
    }
}

qint64 LogoCache::totalBytes() const {
    QMutexLocker locker(&m_mutex);
    qint64 total = 0;
    const QFileInfoList files = QDir(m_directory).entryInfoList(QDir::Files);
    for (const QFileInfo &info : files) {
        total += info.size();
    }
    return total;
}

LogoProvider::LogoProvider(const QString &cacheDirectory, QObject *parent) : QObject(parent), m_cache(cacheDirectory) {
    m_workers.setMaxThreadCount(2);
    if (QImageReader::allocationLimit() == 0) {
        QImageReader::setAllocationLimit(256);
    }
    if (!s_instance) {
        s_instance = this;
    }
}

LogoProvider::~LogoProvider() {
    cancelPending();
    m_workers.waitForDone();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

LogoProvider *LogoProvider::create(QQmlEngine *, QJSEngine *) {
    if (!s_instance) {
        s_instance = new LogoProvider(defaultCacheDirectory());
    }
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QString LogoProvider::defaultCacheDirectory() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).filePath(QStringLiteral("logos"));
}

LogoCache *LogoProvider::cache() { return &m_cache; }

int LogoProvider::pendingCount() const { return int(m_jobs.size()); }

void LogoProvider::clearCache() {
    m_workers.start([this] { m_cache.clear(); });
}

void LogoProvider::cancelPending() {
    const QHash<QUrl, Job> jobs = m_jobs;
    m_jobs.clear();
    m_queue.clear();
    m_requests.clear();
    for (auto job = jobs.cbegin(); job != jobs.cend(); ++job) {
        for (const quint64 id : job->waiters) {
            emit finished(id, QImage());
        }
        if (job->reply) {
            job->reply->abort();
        }
    }
}

void LogoProvider::request(quint64 id, const QUrl &address) {
    if (!LogoCache::accepts(address)) {
        emit finished(id, QImage());
        return;
    }
    m_requests.insert(id, address);
    const bool known = m_jobs.contains(address);
    m_jobs[address].waiters.append(id);
    if (!known) {
        m_queue.append(address);
        startNext();
    }
}

void LogoProvider::cancel(quint64 id) {
    const auto request = m_requests.constFind(id);
    if (request == m_requests.constEnd()) {
        return;
    }
    const QUrl address = *request;
    m_requests.erase(request);
    const auto job = m_jobs.find(address);
    if (job == m_jobs.end()) {
        return;
    }
    job->waiters.removeAll(id);
    if (!job->waiters.isEmpty()) {
        return;
    }
    QNetworkReply *reply = job->reply;
    if (m_queue.removeAll(address) > 0) {
        m_jobs.erase(job);
    } else if (reply) {
        reply->abort();
    }
}

void LogoProvider::startNext() {
    while (m_active < maxInFlight && !m_queue.isEmpty()) {
        start(m_queue.takeFirst());
    }
}

void LogoProvider::start(const QUrl &address) {
    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
    }
    QNetworkRequest request(address);
    request.setTransferTimeout(transferTimeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setMaximumRedirectsAllowed(5);
    QNetworkReply *reply = m_network->get(request);
    m_jobs[address].reply = reply;
    ++m_active;

    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64 total) {
        if (received > maxDownloadBytes || total > maxDownloadBytes) {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, address] {
        reply->deleteLater();
        --m_active;
        const auto job = m_jobs.find(address);
        const bool wanted = job != m_jobs.end() && job->reply == reply && !job->waiters.isEmpty();
        if (job != m_jobs.end() && job->reply == reply) {
            job->reply = nullptr;
        }
        const QByteArray data = reply->error() == QNetworkReply::NoError ? reply->read(maxDownloadBytes + 1) : QByteArray();
        if (!wanted) {
            if (job != m_jobs.end() && job->waiters.isEmpty()) {
                m_jobs.erase(job);
            }
        } else if (data.isEmpty() || data.size() > maxDownloadBytes) {
            qDebug() << "Channel logo not loaded:" << reply->errorString();
            complete(address, QImage());
        } else {
            QtConcurrent::run(&m_workers, [this, data, address] {
                const QImage image = LogoCache::decode(data);
                if (!image.isNull()) {
                    m_cache.store(address, image);
                }
                return image;
            }).then(this, [this, address](const QImage &image) { complete(address, image); });
        }
        startNext();
    });
}

void LogoProvider::complete(const QUrl &address, const QImage &image) {
    const Job job = m_jobs.take(address);
    for (const quint64 id : job.waiters) {
        m_requests.remove(id);
        emit finished(id, image);
    }
}

LogoImageProvider::LogoImageProvider(LogoProvider *provider) : m_provider(provider) {}

QQuickImageResponse *LogoImageProvider::requestImageResponse(const QString &id, const QSize &) {
    const QUrl address(QUrl::fromPercentEncoding(id.toUtf8()));
    return new LogoResponse(m_provider, m_nextId.fetch_add(1), address);
}
