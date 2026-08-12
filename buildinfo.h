#ifndef BUILDINFO_H
#define BUILDINFO_H

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class BuildInfo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString ffmpegVersion READ ffmpegVersion CONSTANT)

public:
    explicit BuildInfo(QObject *parent = nullptr);

    static QString qtVersion();

    static QString ffmpegVersion();
};

#endif // BUILDINFO_H
