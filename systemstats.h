#ifndef SYSTEMSTATS_H
#define SYSTEMSTATS_H

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QThread;
class SystemStatsSampler;
struct SystemStatsSnapshot;

class SystemStats : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QString cpuUsage READ cpuUsage NOTIFY statsUpdated)
    Q_PROPERTY(QString ramUsage READ ramUsage NOTIFY statsUpdated)
    Q_PROPERTY(QString ioUsage READ ioUsage NOTIFY statsUpdated)
    Q_PROPERTY(QString gpuUsage READ gpuUsage NOTIFY statsUpdated)
    Q_PROPERTY(QString gpuMemory READ gpuMemory NOTIFY statsUpdated)
    Q_PROPERTY(QString threadCount READ threadCount NOTIFY statsUpdated)
    Q_PROPERTY(double cpuPercent READ cpuPercent NOTIFY statsUpdated)
    Q_PROPERTY(double gpuPercent READ gpuPercent NOTIFY statsUpdated)

public:
    explicit SystemStats(QObject *parent = nullptr);
    ~SystemStats() override;

    bool active() const;
    void setActive(bool active);

    QString cpuUsage() const;
    QString ramUsage() const;
    QString ioUsage() const;
    QString gpuUsage() const;
    QString gpuMemory() const;
    QString threadCount() const;
    double cpuPercent() const;
    double gpuPercent() const;

    Q_INVOKABLE void copyToClipboard(const QString &text) const;

signals:
    void activeChanged();
    void statsUpdated();

private:
    friend class SystemStatsSampler;

    void applySnapshot(const SystemStatsSnapshot &snapshot);

    QThread *m_thread = nullptr;
    SystemStatsSampler *m_sampler = nullptr;
    bool m_active = false;
    int m_generation = 0;

    QString m_cpuUsage = "N/A";
    QString m_ramUsage = "N/A";
    QString m_ioUsage = "N/A";
    QString m_gpuUsage = "N/A";
    QString m_gpuMemory = "N/A";
    QString m_threadCount = "N/A";
    double m_cpuPercent = -1.0;
    double m_gpuPercent = -1.0;
};

#endif // SYSTEMSTATS_H
