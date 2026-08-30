#ifndef SYSTEMSTATS_H
#define SYSTEMSTATS_H

#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class SystemStats : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString cpuUsage READ cpuUsage NOTIFY statsUpdated)
    Q_PROPERTY(QString ramUsage READ ramUsage NOTIFY statsUpdated)
    Q_PROPERTY(QString ioUsage READ ioUsage NOTIFY statsUpdated)
    Q_PROPERTY(QString gpuUsage READ gpuUsage NOTIFY statsUpdated)

public:
    explicit SystemStats(QObject *parent = nullptr);

    QString cpuUsage() const;
    QString ramUsage() const;
    QString ioUsage() const;
    QString gpuUsage() const;

signals:
    void statsUpdated();

private slots:
    void updateStats();

private:
    QTimer *m_timer;

    QString m_cpuUsage = "0.0%";
    QString m_ramUsage = "0 MB / 0 MB";
    QString m_ioUsage = "N/A";
    QString m_gpuUsage = "N/A";

#ifdef Q_OS_LINUX
    unsigned long long m_lastTotalUser = 0;
    unsigned long long m_lastTotalUserLow = 0;
    unsigned long long m_lastTotalSys = 0;
    unsigned long long m_lastTotalIdle = 0;
    unsigned long long m_lastProcessTime = 0;
#endif
};

#endif // SYSTEMSTATS_H
