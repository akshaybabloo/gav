#ifndef PREVIEWIMAGEPROVIDER_H
#define PREVIEWIMAGEPROVIDER_H

#include <QQuickImageProvider>
#include <QImage>
#include <QMap>
#include <QMutex>

class PreviewImageProvider : public QQuickImageProvider {
public:
    PreviewImageProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    
    // Store image with a specific ID and return the string to be used in QML
    void storeImage(const QString &id, const QImage &image);
    
    static PreviewImageProvider *instance();

private:
    QMap<QString, QImage> m_images;
    QMutex m_mutex;
    static PreviewImageProvider *s_instance;
};

#endif // PREVIEWIMAGEPROVIDER_H
