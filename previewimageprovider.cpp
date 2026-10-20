#include "previewimageprovider.h"

#include <algorithm>

PreviewImageProvider *PreviewImageProvider::s_instance = nullptr;

PreviewImageProvider::PreviewImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {
    s_instance = this;
}

PreviewImageProvider *PreviewImageProvider::instance() {
    return s_instance;
}

QImage PreviewImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    QMutexLocker locker(&m_mutex);
    
    // ID might have extra parameters like timestamps, we just want the key
    // For now assuming ID is exact key. 
    // In our usage we will use "preview_TIMESTAMP" or similar unique key.
    
    QImage image;
    if (m_images.contains(id)) {
        image = m_images.value(id);
    } else {
        // Return a transparent placeholder or valid empty image
        return QImage(requestedSize.width() > 0 ? requestedSize.width() : 1, 
                      requestedSize.height() > 0 ? requestedSize.height() : 1, 
                      QImage::Format_ARGB32_Premultiplied);
    }

    if (size) {
        *size = image.size();
    }

    if (requestedSize.width() > 0 && requestedSize.height() > 0) {
        return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return image;
}

namespace {

constexpr int maxImagesPerOwner = 20;

QString ownerPrefix(const QString &owner) { return owner + QLatin1Char('_'); }

QString ownerOf(const QString &id) { return id.section(QLatin1Char('_'), 0, 0); }

void removeOwner(QMap<QString, QImage> &images, const QString &owner) {
    const QString prefix = ownerPrefix(owner);
    images.removeIf([&prefix](const QMap<QString, QImage>::iterator &it) { return it.key().startsWith(prefix); });
}

} // namespace

QString PreviewImageProvider::imageId(const QString &owner, qint64 stamp) {
    return ownerPrefix(owner) + QStringLiteral("preview_%1").arg(stamp);
}

void PreviewImageProvider::storeImage(const QString &id, const QImage &image) {
    QMutexLocker locker(&m_mutex);
    const QString prefix = ownerPrefix(ownerOf(id));
    const auto owned = std::count_if(m_images.keyBegin(), m_images.keyEnd(), [&prefix](const QString &key) { return key.startsWith(prefix); });
    if (owned >= maxImagesPerOwner) {
        removeOwner(m_images, ownerOf(id));
    }
    m_images.insert(id, image);
}

void PreviewImageProvider::clearImages(const QString &owner) {
    QMutexLocker locker(&m_mutex);
    removeOwner(m_images, owner);
}
