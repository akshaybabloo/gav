#ifndef SYSTEMSTATS_H
#define SYSTEMSTATS_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QHash>
#include <memory>
#include <QtQml/qqmlregistration.h>

struct NvmlHandler;

class SystemStats : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QString cpuUsage READ cpuUsage NOTIFY statsUpdated)
    Q_PROPERTY(QString ramUsage READ ramUsage NOTIFY statsUpdated)
    Q_PROPERTY(QString ioUsage READ ioUsage NOTIFY statsUpdated)
    Q_PROPERTY(QString gpuUsage READ gpuUsage NOTIFY statsUpdated)

public:
    explicit SystemStats(QObject *parent = nullptr);
    ~SystemStats() override;

    bool active() const;
    void setActive(bool active);

    QString cpuUsage() const;
    QString ramUsage() const;
    QString ioUsage() const;
    QString gpuUsage() const;

signals:
    void activeChanged();
    void statsUpdated();

private slots:
    void updateStats();

private:
    void resetSamples();
    void updateCpuStats();
    void updateRamStats();
    void updateIoStats();
    void updateGpuStats();
#ifdef Q_OS_LINUX
    double drmGpuUsage();
#endif

    QTimer *m_timer;
    bool m_active = false;

    QString m_cpuUsage = "N/A";
    QString m_ramUsage = "N/A";
    QString m_ioUsage = "N/A";
    QString m_gpuUsage = "N/A";

    qint64 m_lastCpuNs = 0;
    QElapsedTimer m_cpuTimer;

    unsigned long long m_lastReadBytes = 0;
    unsigned long long m_lastWriteBytes = 0;
    QElapsedTimer m_ioTimer;

    std::unique_ptr<NvmlHandler> m_nvml;
#ifdef Q_OS_LINUX
    QHash<QByteArray, unsigned long long> m_lastDrmEngineTime;
    QElapsedTimer m_drmTimer;
#endif
};

#endif // SYSTEMSTATS_H
