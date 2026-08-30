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
    Q_PROPERTY(bool isDebugBuild READ isDebugBuild CONSTANT)

public:
    explicit BuildInfo(QObject *parent = nullptr);

    static QString qtVersion();

    static QString ffmpegVersion();
    
    static bool isDebugBuild();
};

#endif // BUILDINFO_H
