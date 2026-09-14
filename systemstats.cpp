#include "systemstats.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <QThread>
#include <QDir>
#include <QLibrary>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef Q_OS_MAC
#include <libproc.h>
#include <unistd.h>
#endif

typedef void *nvmlDevice_t;
typedef struct {
    unsigned int gpu;
    unsigned int memory;
} nvmlUtilization_t;

typedef int (*nvmlInit_f)();
typedef int (*nvmlShutdown_f)();
typedef int (*nvmlDeviceGetCount_f)(unsigned int *);
typedef int (*nvmlDeviceGetHandleByIndex_f)(unsigned int, nvmlDevice_t *);
typedef int (*nvmlDeviceGetUtilizationRates_f)(nvmlDevice_t, nvmlUtilization_t *);

struct NvmlHandler {
    QLibrary lib;
    bool initialized = false;
    QList<nvmlDevice_t> devices;
    nvmlShutdown_f nvmlShutdown = nullptr;
    nvmlDeviceGetUtilizationRates_f nvmlDeviceGetUtilizationRates = nullptr;

    NvmlHandler() {
        init();
    }

    ~NvmlHandler() {
        if (initialized && nvmlShutdown) {
            nvmlShutdown();
        }
    }

    void init() {
        lib.setFileName("nvidia-ml");
        if (!lib.load()) {
            lib.setFileName("libnvidia-ml.so.1");
            if (!lib.load()) {
                lib.setFileName("libnvidia-ml.so");
                if (!lib.load()) {
                    lib.setFileName("nvml");
                    if (!lib.load()) {
                        return;
                    }
                }
            }
        }

        auto nvmlInit = reinterpret_cast<nvmlInit_f>(lib.resolve("nvmlInit_v2"));
        if (!nvmlInit) {
            nvmlInit = reinterpret_cast<nvmlInit_f>(lib.resolve("nvmlInit"));
        }
        if (!nvmlInit || nvmlInit() != 0) {
            return;
        }

        nvmlShutdown = reinterpret_cast<nvmlShutdown_f>(lib.resolve("nvmlShutdown"));
        auto nvmlDeviceGetCount = reinterpret_cast<nvmlDeviceGetCount_f>(lib.resolve("nvmlDeviceGetCount_v2"));
        if (!nvmlDeviceGetCount) {
            nvmlDeviceGetCount = reinterpret_cast<nvmlDeviceGetCount_f>(lib.resolve("nvmlDeviceGetCount"));
        }
        auto nvmlDeviceGetHandleByIndex = reinterpret_cast<nvmlDeviceGetHandleByIndex_f>(lib.resolve("nvmlDeviceGetHandleByIndex_v2"));
        if (!nvmlDeviceGetHandleByIndex) {
            nvmlDeviceGetHandleByIndex = reinterpret_cast<nvmlDeviceGetHandleByIndex_f>(lib.resolve("nvmlDeviceGetHandleByIndex"));
        }
        nvmlDeviceGetUtilizationRates = reinterpret_cast<nvmlDeviceGetUtilizationRates_f>(lib.resolve("nvmlDeviceGetUtilizationRates"));

        if (!nvmlDeviceGetCount || !nvmlDeviceGetHandleByIndex || !nvmlDeviceGetUtilizationRates) {
            return;
        }

        unsigned int count = 0;
        if (nvmlDeviceGetCount(&count) == 0 && count > 0) {
            for (unsigned int i = 0; i < count; ++i) {
                nvmlDevice_t dev = nullptr;
                if (nvmlDeviceGetHandleByIndex(i, &dev) == 0 && dev) {
                    devices.append(dev);
                }
            }
        }

        initialized = !devices.isEmpty();
    }

    bool isLoaded() const {
        return initialized;
    }

    double getGpuUsage() {
        if (!initialized || !nvmlDeviceGetUtilizationRates) {
            return -1.0;
        }
        double maxUtil = -1.0;
        for (void *device : devices) {
            nvmlUtilization_t util = {0, 0};
            if (nvmlDeviceGetUtilizationRates(device, &util) == 0) {
                maxUtil = qMax(maxUtil, static_cast<double>(util.gpu));
            }
        }
        return maxUtil;
    }
};

static QString formatBytesPerSec(double bytesPerSec) {
    if (bytesPerSec < 1024.0) {
        return QString::number(bytesPerSec, 'f', 0) + " B/s";
    } else if (bytesPerSec < 1024.0 * 1024.0) {
        return QString::number(bytesPerSec / 1024.0, 'f', 1) + " KB/s";
    } else if (bytesPerSec < 1024.0 * 1024.0 * 1024.0) {
        return QString::number(bytesPerSec / (1024.0 * 1024.0), 'f', 1) + " MB/s";
    } else {
        return QString::number(bytesPerSec / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB/s";
    }
}

SystemStats::SystemStats(QObject *parent)
    : QObject(parent), m_nvml(std::make_unique<NvmlHandler>()) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SystemStats::updateStats);
    m_timer->start(1000); // Update every 1 second
    updateStats(); // Initial update
}

SystemStats::~SystemStats() = default;

QString SystemStats::cpuUsage() const { return m_cpuUsage; }
QString SystemStats::ramUsage() const { return m_ramUsage; }
QString SystemStats::ioUsage() const { return m_ioUsage; }
QString SystemStats::gpuUsage() const { return m_gpuUsage; }

void SystemStats::updateCpuStats() {
#ifdef Q_OS_LINUX
    // CPU Usage (Process)
    unsigned long long totalSystemTime = 0;
    QFile statFile("/proc/stat");
    if (statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&statFile);
        QString line = in.readLine();
        if (line.startsWith("cpu ")) {
            QStringList values = line.mid(4).simplified().split(' ');
            if (values.size() >= 4) {
                unsigned long long user = values[0].toULongLong();
                unsigned long long nice = values[1].toULongLong();
                unsigned long long sys = values[2].toULongLong();
                unsigned long long idle = values[3].toULongLong();
                totalSystemTime = user + nice + sys + idle;
            }
        }
        statFile.close();
    }

    unsigned long long processTime = 0;
    QFile selfStatFile("/proc/self/stat");
    if (selfStatFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&selfStatFile);
        QString line = in.readAll();
        int rparen = line.lastIndexOf(')');
        if (rparen != -1) {
            QString remainder = line.mid(rparen + 2);
            QStringList values = remainder.split(' ');
            if (values.size() >= 13) {
                unsigned long long utime = values[11].toULongLong();
                unsigned long long stime = values[12].toULongLong();
                processTime = utime + stime;
            }
        }
        selfStatFile.close();
    }

    if (m_lastTotalUser > 0 && totalSystemTime > m_lastTotalUser && processTime >= m_lastProcessTime) {
        unsigned long long systemDelta = totalSystemTime - m_lastTotalUser;
        unsigned long long processDelta = processTime - m_lastProcessTime;
        if (systemDelta > 0) {
            int numCores = QThread::idealThreadCount();
            double percent = ((double)processDelta / systemDelta) * 100.0 * numCores;
            m_cpuUsage = QString::number(percent, 'f', 1) + "%";
        }
    }

    m_lastTotalUser = totalSystemTime;
    m_lastProcessTime = processTime;
#endif
}

void SystemStats::updateRamStats() {
#ifdef Q_OS_LINUX
    // RAM Usage (Process)
    QFile selfStatusFile("/proc/self/status");
    if (selfStatusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&selfStatusFile);
        unsigned long long vmRSS = 0;
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith("VmRSS:")) {
                vmRSS = line.split(QRegularExpression("\\s+"))[1].toULongLong();
                break;
            }
        }
        selfStatusFile.close();
        if (vmRSS > 0) {
            m_ramUsage = QString::number(vmRSS / 1024.0, 'f', 1) + " MB";
        }
    }
#elif defined(Q_OS_WIN)
    // Basic windows RAM usage
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
    DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
    m_ramUsage = QString::number(physMemUsed / (1024 * 1024)) + " MB / " + QString::number(totalPhysMem / (1024 * 1024)) + " MB";
#endif
}

void SystemStats::updateIoStats() {
    unsigned long long currentRead = 0;
    unsigned long long currentWrite = 0;
    bool ioAvailable = false;

#ifdef Q_OS_LINUX
    QFile ioFile("/proc/self/io");
    if (ioFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&ioFile);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith("rchar:")) {
                bool ok = false;
                currentRead = line.mid(6).trimmed().toULongLong(&ok);
                if (ok) ioAvailable = true;
            } else if (line.startsWith("wchar:")) {
                bool ok = false;
                unsigned long long w = line.mid(6).trimmed().toULongLong(&ok);
                if (ok) currentWrite = w;
            }
        }
        ioFile.close();
    }
#elif defined(Q_OS_WIN)
    IO_COUNTERS ioCounters;
    if (GetProcessIoCounters(GetCurrentProcess(), &ioCounters)) {
        currentRead = ioCounters.ReadTransferCount;
        currentWrite = ioCounters.WriteTransferCount;
        ioAvailable = true;
    }
#elif defined(Q_OS_MAC)
    struct rusage_info_v2 rusage;
    if (proc_pid_rusage(getpid(), RUSAGE_INFO_V2, reinterpret_cast<rusage_info_t *>(&rusage)) == 0) {
        currentRead = rusage.ri_diskio_bytesread;
        currentWrite = rusage.ri_diskio_byteswritten;
        ioAvailable = true;
    }
#endif

    if (ioAvailable) {
        qint64 elapsedMs = m_ioTimer.isValid() ? m_ioTimer.restart() : 0;
        if (!m_ioTimer.isValid()) {
            m_ioTimer.start();
        }

        if (m_hasLastIo && elapsedMs > 0) {
            double elapsedSec = elapsedMs / 1000.0;
            double readRate = (currentRead >= m_lastReadBytes) ? (currentRead - m_lastReadBytes) / elapsedSec : 0.0;
            double writeRate = (currentWrite >= m_lastWriteBytes) ? (currentWrite - m_lastWriteBytes) / elapsedSec : 0.0;
            m_ioUsage = QString("R: %1 | W: %2").arg(formatBytesPerSec(readRate), formatBytesPerSec(writeRate));
        } else {
            m_ioUsage = "R: 0 B/s | W: 0 B/s";
        }
        m_lastReadBytes = currentRead;
        m_lastWriteBytes = currentWrite;
        m_hasLastIo = true;
    } else {
        m_ioUsage = "N/A";
    }
}

void SystemStats::updateGpuStats() {
    double bestUsage = -1.0;
    bool anyGpuAvailable = false;

#ifdef Q_OS_LINUX
    // 1. Check DRM fdinfo for client GPU usage (Intel / AMD / Nouveau / etc.)
    double drmGpuUsage = -1.0;
    bool drmAvailable = false;
    QDir fdinfoDir("/proc/self/fdinfo");
    if (fdinfoDir.exists()) {
        const QStringList entries = fdinfoDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        QMap<QString, unsigned long long> currentDrmEngineTime;
        bool hasDrmFd = false;

        for (const QString &entry : entries) {
            QFile file(fdinfoDir.filePath(entry));
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                QString clientId;
                bool isDrm = false;
                QList<QPair<QString, unsigned long long>> engines;

                while (!in.atEnd()) {
                    QString line = in.readLine();
                    if (line.startsWith("drm-driver:")) {
                        isDrm = true;
                        hasDrmFd = true;
                    } else if (line.startsWith("drm-client-id:")) {
                        clientId = line.mid(14).trimmed();
                    } else if (line.startsWith("drm-engine-") && line.contains("ns")) {
                        int colon = line.indexOf(':');
                        if (colon != -1) {
                            QString engine = line.left(colon).trimmed();
                            QString valStr = line.mid(colon + 1).trimmed();
                            int nsIdx = valStr.indexOf("ns");
                            if (nsIdx != -1) {
                                valStr = valStr.left(nsIdx).trimmed();
                            }
                            bool ok = false;
                            unsigned long long ns = valStr.toULongLong(&ok);
                            if (ok) {
                                engines.append(qMakePair(engine, ns));
                            }
                        }
                    }
                }
                file.close();

                if (isDrm) {
                    QString prefix = clientId.isEmpty() ? entry : clientId;
                    for (const auto &pair : engines) {
                        QString key = prefix + "_" + pair.first;
                        currentDrmEngineTime[key] = pair.second;
                    }
                }
            }
        }

        if (hasDrmFd) {
            qint64 elapsedNs = m_drmTimer.isValid() ? m_drmTimer.restart() : 0;
            if (!m_drmTimer.isValid()) {
                m_drmTimer.start();
            }

            if (elapsedNs > 0 && !m_lastDrmEngineTime.isEmpty()) {
                unsigned long long totalDeltaNs = 0;
                for (auto it = currentDrmEngineTime.constBegin(); it != currentDrmEngineTime.constEnd(); ++it) {
                    if (m_lastDrmEngineTime.contains(it.key())) {
                        unsigned long long prev = m_lastDrmEngineTime.value(it.key());
                        if (it.value() >= prev) {
                            totalDeltaNs += (it.value() - prev);
                        }
                    }
                }
                double usage = (totalDeltaNs / static_cast<double>(elapsedNs)) * 100.0;
                if (usage > 100.0) {
                    usage = 100.0;
                }
                drmGpuUsage = usage;
                drmAvailable = true;
            } else {
                drmGpuUsage = 0.0;
                drmAvailable = true;
            }
            m_lastDrmEngineTime = currentDrmEngineTime;
        }
    }

    if (drmAvailable && drmGpuUsage >= 0.0) {
        bestUsage = qMax(bestUsage, drmGpuUsage);
        anyGpuAvailable = true;
    }

    // 2. Check AMD sysfs gpu_busy_percent
    double amdGpuUsage = -1.0;
    bool amdAvailable = false;
    QDir drmDir("/sys/class/drm");
    if (drmDir.exists()) {
        const QStringList cards = drmDir.entryList(QStringList() << "card*", QDir::Dirs);
        for (const QString &card : cards) {
            QFile busyFile(drmDir.filePath(card) + "/device/gpu_busy_percent");
            if (busyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&busyFile);
                bool ok = false;
                double val = in.readLine().trimmed().toDouble(&ok);
                busyFile.close();
                if (ok && val >= 0.0) {
                    amdGpuUsage = qMax(amdGpuUsage, qMin(100.0, val));
                    amdAvailable = true;
                }
            }
        }
    }

    if (amdAvailable && amdGpuUsage >= 0.0) {
        bestUsage = qMax(bestUsage, amdGpuUsage);
        anyGpuAvailable = true;
    }
#endif

    // 3. Check NVIDIA NVML (Linux and Windows)
    if (m_nvml && m_nvml->isLoaded()) {
        double nvmlUsage = m_nvml->getGpuUsage();
        if (nvmlUsage >= 0.0) {
            bestUsage = qMax(bestUsage, nvmlUsage);
            anyGpuAvailable = true;
        }
    }

    if (anyGpuAvailable && bestUsage >= 0.0) {
        m_gpuUsage = QString::number(bestUsage, 'f', 1) + "%";
    } else {
        m_gpuUsage = "N/A";
    }
}

void SystemStats::updateStats() {
    updateCpuStats();
    updateRamStats();
    updateIoStats();
    updateGpuStats();

    emit statsUpdated();
}

