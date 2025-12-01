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

    Q_INVOKABLE static void toCollage(const QList<QUrl>& paths);

private:
    static ImageMeta collectImages(const QUrl& path);
    static QImage drawCollage(const ImageMeta& meta);


signals:
};

#endif // COLLAGE_H
