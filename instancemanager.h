#ifndef INSTANCEMANAGER_H
#define INSTANCEMANAGER_H

#include <QList>
#include <QLocalServer>
#include <QLockFile>
#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

class InstanceManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit InstanceManager(QObject *parent = nullptr);
    ~InstanceManager() override;

    static InstanceManager *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    bool start(const QList<QUrl> &urls);

    Q_INVOKABLE QList<QUrl> takePendingUrls();

signals:
    void urlsPending();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static bool forward(const QList<QUrl> &urls);
    void listen();
    void enqueue(const QList<QUrl> &urls);

    QLockFile m_lock;
    QLocalServer m_server;
    QList<QUrl> m_pending;
};

#endif // INSTANCEMANAGER_H
