#include "instancemanager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDeadlineTimer>
#include <QDir>
#include <QFileOpenEvent>
#include <QJSEngine>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QThread>
#include <memory>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
InstanceManager *s_instance = nullptr;

QString instanceKey() {
    const QString user = qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME"));
    const QByteArray hash = QCryptographicHash::hash(user.toUtf8(), QCryptographicHash::Sha1).toHex().left(12);
    return QStringLiteral("gav-") + QString::fromLatin1(hash);
}

QString lockFilePath() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(instanceKey() + ".lock");
}
}

InstanceManager::InstanceManager(QObject *parent)
    : QObject(parent), m_lock(lockFilePath()) {
    s_instance = this;
    m_lock.setStaleLockTime(0);
    QCoreApplication::instance()->installEventFilter(this);
}

InstanceManager::~InstanceManager() {
    if (s_instance == this)
        s_instance = nullptr;
}

InstanceManager *InstanceManager::create(QQmlEngine *, QJSEngine *) {
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool InstanceManager::start(const QList<QUrl> &urls) {
    const QDeadlineTimer deadline(5000);
    while (!m_lock.tryLock()) {
        if (urls.isEmpty())
            return true;
        if (forward(urls)) {
            qDebug() << "Forwarded" << urls.size() << "file(s) to the running instance";
            return false;
        }
        if (deadline.hasExpired()) {
            enqueue(urls);
            return true;
        }
        QThread::msleep(100);
    }
    listen();
    enqueue(urls);
    return true;
}

QList<QUrl> InstanceManager::takePendingUrls() {
    return std::exchange(m_pending, {});
}

bool InstanceManager::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::FileOpen) {
        enqueue({static_cast<QFileOpenEvent *>(event)->url()});
        return true;
    }
    return QObject::eventFilter(watched, event);
}

bool InstanceManager::forward(const QList<QUrl> &urls) {
    QLocalSocket socket;
    socket.connectToServer(instanceKey());
    if (!socket.waitForConnected(500))
        return false;

#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
#endif

    QByteArray payload;
    for (const QUrl &url : urls)
        payload += url.toEncoded() + '\n';
    payload += '\n';

    socket.write(payload);
    socket.waitForBytesWritten(1000);
    return socket.waitForReadyRead(10000) || socket.state() == QLocalSocket::ConnectedState;
}

void InstanceManager::listen() {
    const QString key = instanceKey();
    QLocalServer::removeServer(key);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(key)) {
        qWarning() << "Could not listen for other instances:" << m_server.errorString();
        return;
    }

    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server.nextPendingConnection()) {
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QLocalSocket::readyRead, this, [this, socket, buffer] {
                buffer->append(socket->readAll());
                if (!buffer->endsWith("\n\n"))
                    return;

                QList<QUrl> urls;
                for (const QByteArray &line : buffer->split('\n')) {
                    if (!line.isEmpty())
                        urls.append(QUrl::fromEncoded(line));
                }
                buffer->clear();

                socket->write("\n");
                socket->flush();
                enqueue(urls);
            });
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
}

void InstanceManager::enqueue(const QList<QUrl> &urls) {
    if (urls.isEmpty())
        return;
    m_pending.append(urls);
    emit urlsPending();
}
