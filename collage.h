#ifndef COLLAGE_H
#define COLLAGE_H

#include <QQuickItem>

class Collage : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit Collage();

    Q_INVOKABLE void toCollage(QList<QUrl> path);

private:
    QList<QImage> collectImages(QUrl path);
    QImage drawCollage(QList<QImage> clips, QString metaData);


signals:
};

#endif // COLLAGE_H
