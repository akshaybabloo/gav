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
    ImageMeta collectImages(const QUrl& path);
    QImage drawCollage(QList<QImage> clips, QString metaData);


signals:
};

#endif // COLLAGE_H
