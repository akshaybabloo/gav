#include "systemstats.h"

#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QLibrary>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

#ifdef Q_OS_UNIX
#include <sys/resource.h>
#include <unistd.h>
#endif

#ifdef Q_OS_MAC
#include <libproc.h>
#endif

typedef void *nvmlDevice_t;
typedef struct {
    unsigned int pid;
    unsigned long long timeStamp;
    unsigned int smUtil;
    unsigned int memUtil;
    unsigned int encUtil;
    unsigned int decUtil;
} nvmlProcessUtilizationSample_t;

typedef int (*nvmlInit_f)();
typedef int (*nvmlShutdown_f)();
typedef int (*nvmlDeviceGetCount_f)(unsigned int *);
typedef int (*nvmlDeviceGetHandleByIndex_f)(unsigned int, nvmlDevice_t *);
typedef int (*nvmlDeviceGetProcessUtilization_f)(nvmlDevice_t, nvmlProcessUtilizationSample_t *, unsigned int *, unsigned long long);

static constexpr int NVML_SUCCESS = 0;
static constexpr int NVML_ERROR_NOT_FOUND = 6;
static constexpr int NVML_ERROR_INSUFFICIENT_SIZE = 7;

struct NvmlHandler {
    QLibrary lib;
    bool initialized = false;
    QList<nvmlDevice_t> devices;
    QList<unsigned long long> lastSeenTimeStamps;
    nvmlShutdown_f nvmlShutdown = nullptr;
    nvmlDeviceGetProcessUtilization_f nvmlDeviceGetProcessUtilization = nullptr;

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
        if (!nvmlInit || nvmlInit() != NVML_SUCCESS) {
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
        nvmlDeviceGetProcessUtilization = reinterpret_cast<nvmlDeviceGetProcessUtilization_f>(lib.resolve("nvmlDeviceGetProcessUtilization"));

        if (!nvmlDeviceGetCount || !nvmlDeviceGetHandleByIndex || !nvmlDeviceGetProcessUtilization) {
            if (nvmlShutdown) {
                nvmlShutdown();
            }
            return;
        }

        unsigned int count = 0;
        if (nvmlDeviceGetCount(&count) == NVML_SUCCESS) {
            for (unsigned int i = 0; i < count; ++i) {
                nvmlDevice_t dev = nullptr;
                if (nvmlDeviceGetHandleByIndex(i, &dev) == NVML_SUCCESS && dev) {
                    devices.append(dev);
                    lastSeenTimeStamps.append(0);
                }
            }
        }

        initialized = true;
    }

    double processUsage(unsigned int pid) {
        if (!initialized) {
            return -1.0;
        }
        double best = -1.0;
        for (qsizetype i = 0; i < devices.size(); ++i) {
            unsigned int count = 0;
            int ret = nvmlDeviceGetProcessUtilization(devices[i], nullptr, &count, lastSeenTimeStamps[i]);
            if (ret == NVML_ERROR_NOT_FOUND || (ret == NVML_SUCCESS && count == 0)) {
                best = qMax(best, 0.0);
                continue;
            }
            if (ret != NVML_ERROR_INSUFFICIENT_SIZE || count == 0) {
                continue;
            }

            std::vector<nvmlProcessUtilizationSample_t> samples(count);
            ret = nvmlDeviceGetProcessUtilization(devices[i], samples.data(), &count, lastSeenTimeStamps[i]);
            if (ret == NVML_ERROR_NOT_FOUND) {
                best = qMax(best, 0.0);
                continue;
            }
            if (ret != NVML_SUCCESS) {
                continue;
            }

            unsigned int usage = 0;
            for (unsigned int s = 0; s < count && s < samples.size(); ++s) {
                const auto &sample = samples[s];
                lastSeenTimeStamps[i] = qMax(lastSeenTimeStamps[i], sample.timeStamp);
                if (sample.pid == pid) {
                    usage = qMax(usage, qMax(sample.smUtil, qMax(sample.encUtil, sample.decUtil)));
                }
            }
            best = qMax(best, qMin(100.0, static_cast<double>(usage)));
        }
        return best;
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

#ifdef Q_OS_LINUX
static QByteArray readProcFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}
#endif

static qint64 processCpuTimeNs() {
#if defined(Q_OS_WIN)
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (!GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
        return -1;
    }
    auto toNs = [](const FILETIME &ft) {
        return static_cast<qint64>((static_cast<quint64>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime) * 100;
    };
    return toNs(kernelTime) + toNs(userTime);
#elif defined(Q_OS_UNIX)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return -1;
    }
    const qint64 us = (static_cast<qint64>(usage.ru_utime.tv_sec) + usage.ru_stime.tv_sec) * 1000000
                      + usage.ru_utime.tv_usec + usage.ru_stime.tv_usec;
    return us * 1000;
#else
    return -1;
#endif
}

SystemStats::SystemStats(QObject *parent)
    : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &SystemStats::updateStats);
}

SystemStats::~SystemStats() = default;

bool SystemStats::active() const { return m_active; }

void SystemStats::setActive(bool active) {
    if (m_active == active) {
        return;
    }
    m_active = active;
    if (m_active) {
        if (!m_nvml) {
            m_nvml = std::make_unique<NvmlHandler>();
        }
        resetSamples();
        updateStats();
        m_timer->start();
    } else {
        m_timer->stop();
    }
    emit activeChanged();
}

QString SystemStats::cpuUsage() const { return m_cpuUsage; }
QString SystemStats::ramUsage() const { return m_ramUsage; }
QString SystemStats::ioUsage() const { return m_ioUsage; }
QString SystemStats::gpuUsage() const { return m_gpuUsage; }

void SystemStats::resetSamples() {
    m_cpuUsage = "N/A";
    m_ramUsage = "N/A";
    m_ioUsage = "N/A";
    m_gpuUsage = "N/A";
    m_cpuTimer.invalidate();
    m_ioTimer.invalidate();
#ifdef Q_OS_LINUX
    m_lastDrmEngineTime.clear();
    m_drmTimer.invalidate();
#endif
}

void SystemStats::updateCpuStats() {
    const qint64 cpuNs = processCpuTimeNs();
    if (cpuNs < 0) {
        m_cpuUsage = "N/A";
        return;
    }

    const bool hasBaseline = m_cpuTimer.isValid();
    const qint64 elapsedNs = hasBaseline ? m_cpuTimer.nsecsElapsed() : 0;
    m_cpuTimer.start();

    if (hasBaseline && elapsedNs > 0 && cpuNs >= m_lastCpuNs) {
        const double percent = static_cast<double>(cpuNs - m_lastCpuNs) * 100.0 / static_cast<double>(elapsedNs);
        m_cpuUsage = QString::number(percent, 'f', 1) + "%";
    }
    m_lastCpuNs = cpuNs;
}

void SystemStats::updateRamStats() {
    unsigned long long residentBytes = 0;

#if defined(Q_OS_LINUX)
    const QList<QByteArray> fields = readProcFile("/proc/self/statm").simplified().split(' ');
    if (fields.size() >= 2) {
        residentBytes = fields[1].toULongLong() * static_cast<unsigned long long>(sysconf(_SC_PAGESIZE));
    }
#elif defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS counters;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        residentBytes = counters.WorkingSetSize;
    }
#elif defined(Q_OS_MAC)
    rusage_info_v2 usage;
    if (proc_pid_rusage(getpid(), RUSAGE_INFO_V2, reinterpret_cast<rusage_info_t *>(&usage)) == 0) {
        residentBytes = usage.ri_resident_size;
    }
#endif

    if (residentBytes > 0) {
        m_ramUsage = QString::number(residentBytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    } else {
        m_ramUsage = "N/A";
    }
}

void SystemStats::updateIoStats() {
    unsigned long long currentRead = 0;
    unsigned long long currentWrite = 0;
    bool ioAvailable = false;

#if defined(Q_OS_LINUX)
    const QList<QByteArray> lines = readProcFile("/proc/self/io").split('\n');
    bool hasRead = false;
    bool hasWrite = false;
    for (const QByteArray &line : lines) {
        if (line.startsWith("rchar:")) {
            currentRead = line.mid(6).trimmed().toULongLong(&hasRead);
        } else if (line.startsWith("wchar:")) {
            currentWrite = line.mid(6).trimmed().toULongLong(&hasWrite);
        }
    }
    ioAvailable = hasRead && hasWrite;
#elif defined(Q_OS_WIN)
    IO_COUNTERS ioCounters;
    if (GetProcessIoCounters(GetCurrentProcess(), &ioCounters)) {
        currentRead = ioCounters.ReadTransferCount;
        currentWrite = ioCounters.WriteTransferCount;
        ioAvailable = true;
    }
#elif defined(Q_OS_MAC)
    rusage_info_v2 usage;
    if (proc_pid_rusage(getpid(), RUSAGE_INFO_V2, reinterpret_cast<rusage_info_t *>(&usage)) == 0) {
        currentRead = usage.ri_diskio_bytesread;
        currentWrite = usage.ri_diskio_byteswritten;
        ioAvailable = true;
    }
#endif

    if (!ioAvailable) {
        m_ioUsage = "N/A";
        m_ioTimer.invalidate();
        return;
    }

    const bool hasBaseline = m_ioTimer.isValid();
    const qint64 elapsedNs = hasBaseline ? m_ioTimer.nsecsElapsed() : 0;
    m_ioTimer.start();

    if (hasBaseline && elapsedNs > 0) {
        const double elapsedSec = static_cast<double>(elapsedNs) / 1e9;
        const double readRate = currentRead >= m_lastReadBytes ? (currentRead - m_lastReadBytes) / elapsedSec : 0.0;
        const double writeRate = currentWrite >= m_lastWriteBytes ? (currentWrite - m_lastWriteBytes) / elapsedSec : 0.0;
        m_ioUsage = QString("R: %1 | W: %2").arg(formatBytesPerSec(readRate), formatBytesPerSec(writeRate));
    }
    m_lastReadBytes = currentRead;
    m_lastWriteBytes = currentWrite;
}

#ifdef Q_OS_LINUX
double SystemStats::drmGpuUsage() {
    QDir fdinfoDir("/proc/self/fdinfo");
    const QStringList entries = fdinfoDir.entryList(QDir::Files);
    QHash<QByteArray, unsigned long long> currentEngineTime;

    for (const QString &entry : entries) {
        const QByteArray content = readProcFile(fdinfoDir.filePath(entry));
        if (!content.contains("drm-driver:")) {
            continue;
        }

        QByteArray device;
        QByteArray clientId = entry.toLatin1();
        QList<QPair<QByteArray, unsigned long long>> engines;
        for (const QByteArray &line : content.split('\n')) {
            if (line.startsWith("drm-pdev:")) {
                device = line.mid(9).trimmed();
            } else if (line.startsWith("drm-client-id:")) {
                clientId = line.mid(14).trimmed();
            } else if (line.startsWith("drm-engine-")) {
                const qsizetype colon = line.indexOf(':');
                if (colon == -1) {
                    continue;
                }
                QByteArray value = line.mid(colon + 1).trimmed();
                if (!value.endsWith("ns")) {
                    continue;
                }
                value.chop(2);
                bool ok = false;
                const unsigned long long ns = value.trimmed().toULongLong(&ok);
                if (ok) {
                    engines.append({line.left(colon), ns});
                }
            }
        }

        for (const auto &engine : engines) {
            currentEngineTime.insert(device + '|' + engine.first + '|' + clientId, engine.second);
        }
    }

    const bool hasBaseline = m_drmTimer.isValid();
    const qint64 elapsedNs = hasBaseline ? m_drmTimer.nsecsElapsed() : 0;
    m_drmTimer.start();

    double usage = currentEngineTime.isEmpty() ? -1.0 : 0.0;
    if (hasBaseline && elapsedNs > 0) {
        QHash<QByteArray, unsigned long long> busyByEngine;
        for (auto it = currentEngineTime.constBegin(); it != currentEngineTime.constEnd(); ++it) {
            const auto prev = m_lastDrmEngineTime.constFind(it.key());
            if (prev != m_lastDrmEngineTime.constEnd() && it.value() >= prev.value()) {
                busyByEngine[it.key().left(it.key().lastIndexOf('|'))] += it.value() - prev.value();
            }
        }
        for (const unsigned long long busyNs : std::as_const(busyByEngine)) {
            usage = qMax(usage, qMin(100.0, static_cast<double>(busyNs) * 100.0 / static_cast<double>(elapsedNs)));
        }
    }
    m_lastDrmEngineTime = currentEngineTime;
    return usage;
}
#endif

void SystemStats::updateGpuStats() {
    double usage = -1.0;

#ifdef Q_OS_LINUX
    usage = drmGpuUsage();
#endif

    if (m_nvml) {
        usage = qMax(usage, m_nvml->processUsage(static_cast<unsigned int>(QCoreApplication::applicationPid())));
    }

    m_gpuUsage = usage >= 0.0 ? QString::number(usage, 'f', 1) + "%" : "N/A";
}

void SystemStats::updateStats() {
    updateCpuStats();
    updateRamStats();
    updateIoStats();
    updateGpuStats();

    emit statsUpdated();
}
