#ifndef UPDATES_H
#define UPDATES_H

#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class Updates : public QObject {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit Updates(QObject *parent = nullptr);

    Q_INVOKABLE void checkUpdates();

signals:
    void updateAvailable(const QString &currentVersion, const QString &latestVersion, const QUrl &releaseUrl);
    void upToDate(const QString &currentVersion);
    void checkFailed(const QString &errorMessage);

private:
    QNetworkAccessManager m_manager;
    bool m_checking = false;
};

#endif // UPDATES_H
