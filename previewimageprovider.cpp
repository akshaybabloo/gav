#include "previewimageprovider.h"

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

void PreviewImageProvider::storeImage(const QString &id, const QImage &image) {
    QMutexLocker locker(&m_mutex);
    m_images.insert(id, image);
    
    // Cleanup old images? 
    // For this simple usage where we overwrite often or use unique IDs, 
    // we might want to clear if the map gets too big, or if we are reusing IDs.
    // Let's implement a simple size cap: if > 10 images, remove oldest?
    // But QMap isn't ordered by insertion.
    // Since we are likely using one ID per player or one ID that changes, 
    // let's rely on CustomMediaPlayer reusing the ID or generating new ones.
    // If CustomMediaPlayer reuses "preview" ID it won't force refresh in QML unless URL changes.
    // So CustomMediaPlayer will use "preview_UNIQUE". 
    // We should clear the map periodically or just clear it here if it's too big.
    if (m_images.size() > 20) {
        m_images.clear();
        m_images.insert(id, image); // Keep the new one
    }
}
