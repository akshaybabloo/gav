#ifndef COLLAGE_H
#define COLLAGE_H

#include <QQuickItem>
#include <QImage>

struct ImageTime {
    QImage image;
    qint64 timestamp;
};

struct ImageMeta
{
    QList<ImageTime> image;
    QString name;
    QString duration;
    QString audioCodec;
    QString videoCodes;
    QString size;
    QString resolution;
};

class Collage : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit Collage();

    Q_INVOKABLE void toCollage(const QList<QUrl>& paths);

private:
    static ImageMeta collectImages(const QUrl& path);
    static QImage drawCollage(const ImageMeta& meta);

signals:
    void collageStarted(int total);
    void collageProgress(int index, const QString& inputPath);
    void collageCompleted(int index, const QString& outputPath, bool success);
    void collageFinished(int successCount, int failCount);
};

#endif // COLLAGE_H
