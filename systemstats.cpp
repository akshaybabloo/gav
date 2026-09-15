#include "systemstats.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QDir>
#include <QLibrary>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#endif

#ifdef Q_OS_UNIX
#include <sys/resource.h>
#include <unistd.h>
#endif

#ifdef Q_OS_MAC
#include <libproc.h>
#include <mach/mach.h>
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

typedef struct {
    unsigned int pid;
    unsigned long long usedGpuMemory;
} nvmlProcessInfo_v1_t;

typedef struct {
    unsigned int pid;
    unsigned long long usedGpuMemory;
    unsigned int gpuInstanceId;
    unsigned int computeInstanceId;
} nvmlProcessInfo_v2_t;

typedef int (*nvmlInit_f)();
typedef int (*nvmlShutdown_f)();
typedef int (*nvmlDeviceGetCount_f)(unsigned int *);
typedef int (*nvmlDeviceGetHandleByIndex_f)(unsigned int, nvmlDevice_t *);
typedef int (*nvmlDeviceGetProcessUtilization_f)(nvmlDevice_t, nvmlProcessUtilizationSample_t *, unsigned int *, unsigned long long);
typedef int (*nvmlDeviceGetRunningProcesses_v1_f)(nvmlDevice_t, unsigned int *, nvmlProcessInfo_v1_t *);
typedef int (*nvmlDeviceGetRunningProcesses_v2_f)(nvmlDevice_t, unsigned int *, nvmlProcessInfo_v2_t *);

static constexpr int NVML_SUCCESS = 0;
static constexpr int NVML_ERROR_NOT_FOUND = 6;
static constexpr int NVML_ERROR_INSUFFICIENT_SIZE = 7;
static constexpr unsigned long long NVML_VALUE_NOT_AVAILABLE = ~0ULL;

struct NvmlHandler {
    QLibrary lib;
    bool initialized = false;
    QList<nvmlDevice_t> devices;
    QList<unsigned long long> lastSeenTimeStamps;
    nvmlShutdown_f nvmlShutdown = nullptr;
    nvmlDeviceGetProcessUtilization_f nvmlDeviceGetProcessUtilization = nullptr;
    nvmlDeviceGetRunningProcesses_v2_f nvmlDeviceGetGraphicsRunningProcesses_v2 = nullptr;
    nvmlDeviceGetRunningProcesses_v2_f nvmlDeviceGetComputeRunningProcesses_v2 = nullptr;
    nvmlDeviceGetRunningProcesses_v1_f nvmlDeviceGetGraphicsRunningProcesses = nullptr;
    nvmlDeviceGetRunningProcesses_v1_f nvmlDeviceGetComputeRunningProcesses = nullptr;

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
        nvmlDeviceGetGraphicsRunningProcesses_v2 = reinterpret_cast<nvmlDeviceGetRunningProcesses_v2_f>(lib.resolve("nvmlDeviceGetGraphicsRunningProcesses_v2"));
        nvmlDeviceGetComputeRunningProcesses_v2 = reinterpret_cast<nvmlDeviceGetRunningProcesses_v2_f>(lib.resolve("nvmlDeviceGetComputeRunningProcesses_v2"));
        nvmlDeviceGetGraphicsRunningProcesses = reinterpret_cast<nvmlDeviceGetRunningProcesses_v1_f>(lib.resolve("nvmlDeviceGetGraphicsRunningProcesses"));
        nvmlDeviceGetComputeRunningProcesses = reinterpret_cast<nvmlDeviceGetRunningProcesses_v1_f>(lib.resolve("nvmlDeviceGetComputeRunningProcesses"));

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

    template <typename Info, typename Query>
    static qint64 queryProcessMemory(Query query, nvmlDevice_t device, unsigned int pid) {
        if (!query) {
            return -1;
        }
        unsigned int count = 0;
        const int ret = query(device, &count, nullptr);
        if (ret == NVML_SUCCESS) {
            return 0;
        }
        if (ret != NVML_ERROR_INSUFFICIENT_SIZE) {
            return -1;
        }

        std::vector<Info> infos(count + 8);
        count = static_cast<unsigned int>(infos.size());
        if (query(device, &count, infos.data()) != NVML_SUCCESS) {
            return -1;
        }
        for (unsigned int i = 0; i < count && i < infos.size(); ++i) {
            if (infos[i].pid == pid) {
                return infos[i].usedGpuMemory == NVML_VALUE_NOT_AVAILABLE ? -1 : static_cast<qint64>(infos[i].usedGpuMemory);
            }
        }
        return 0;
    }

    qint64 processMemory(unsigned int pid) {
        if (!initialized) {
            return -1;
        }
        qint64 total = -1;
        for (nvmlDevice_t device : std::as_const(devices)) {
            const qint64 graphics = nvmlDeviceGetGraphicsRunningProcesses_v2
                ? queryProcessMemory<nvmlProcessInfo_v2_t>(nvmlDeviceGetGraphicsRunningProcesses_v2, device, pid)
                : queryProcessMemory<nvmlProcessInfo_v1_t>(nvmlDeviceGetGraphicsRunningProcesses, device, pid);
            const qint64 compute = nvmlDeviceGetComputeRunningProcesses_v2
                ? queryProcessMemory<nvmlProcessInfo_v2_t>(nvmlDeviceGetComputeRunningProcesses_v2, device, pid)
                : queryProcessMemory<nvmlProcessInfo_v1_t>(nvmlDeviceGetComputeRunningProcesses, device, pid);
            const qint64 used = qMax(graphics, compute);
            if (used >= 0) {
                total = qMax(total, qint64(0)) + used;
            }
        }
        return total;
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

static QString formatBytes(qint64 bytes) {
    if (bytes < 1024LL * 1024 * 1024) {
        return QString::number(static_cast<double>(bytes) / (1024.0 * 1024.0), 'f', 1) + " MB";
    }
    return QString::number(static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}

#ifdef Q_OS_LINUX
static bool parseDrmSize(QByteArray value, qint64 &bytes) {
    value = value.trimmed();
    qint64 multiplier = 1;
    if (value.endsWith("GiB")) {
        multiplier = 1024LL * 1024 * 1024;
    } else if (value.endsWith("MiB")) {
        multiplier = 1024LL * 1024;
    } else if (value.endsWith("KiB")) {
        multiplier = 1024LL;
    }
    if (multiplier != 1) {
        value.chop(3);
    }
    bool ok = false;
    const qint64 amount = value.trimmed().toLongLong(&ok);
    if (!ok) {
        return false;
    }
    bytes = amount * multiplier;
    return true;
}

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
        m_nvml.reset();
    }
    emit activeChanged();
}

QString SystemStats::cpuUsage() const { return m_cpuUsage; }
QString SystemStats::ramUsage() const { return m_ramUsage; }
QString SystemStats::ioUsage() const { return m_ioUsage; }
QString SystemStats::gpuUsage() const { return m_gpuUsage; }
QString SystemStats::gpuMemory() const { return m_gpuMemory; }
QString SystemStats::threadCount() const { return m_threadCount; }
double SystemStats::cpuPercent() const { return m_cpuPercent; }
double SystemStats::gpuPercent() const { return m_gpuPercent; }

void SystemStats::copyToClipboard(const QString &text) const {
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}

void SystemStats::resetSamples() {
    m_cpuUsage = "N/A";
    m_ramUsage = "N/A";
    m_ioUsage = "N/A";
    m_gpuUsage = "N/A";
    m_gpuMemory = "N/A";
    m_threadCount = "N/A";
    m_cpuPercent = -1.0;
    m_gpuPercent = -1.0;
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
        m_cpuPercent = -1.0;
        return;
    }

    const bool hasBaseline = m_cpuTimer.isValid();
    const qint64 elapsedNs = hasBaseline ? m_cpuTimer.nsecsElapsed() : 0;
    m_cpuTimer.start();

    if (hasBaseline && elapsedNs > 0 && cpuNs >= m_lastCpuNs) {
        m_cpuPercent = static_cast<double>(cpuNs - m_lastCpuNs) * 100.0 / static_cast<double>(elapsedNs);
        m_cpuUsage = QString::number(m_cpuPercent, 'f', 1) + "%";
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
void SystemStats::updateDrmStats(double &usage, qint64 &memoryBytes) {
    usage = -1.0;
    memoryBytes = -1;

    QDir fdinfoDir("/proc/self/fdinfo");
    const QStringList entries = fdinfoDir.entryList(QDir::Files);
    QHash<QByteArray, unsigned long long> currentEngineTime;
    QHash<QByteArray, qint64> clientMemory;

    for (const QString &entry : entries) {
        const QByteArray content = readProcFile(fdinfoDir.filePath(entry));
        if (!content.contains("drm-driver:")) {
            continue;
        }

        QByteArray device;
        QByteArray clientId = entry.toLatin1();
        QList<QPair<QByteArray, unsigned long long>> engines;
        qint64 residentBytes = -1;
        qint64 legacyBytes = -1;
        for (const QByteArray &line : content.split('\n')) {
            const qsizetype colon = line.indexOf(':');
            if (colon == -1) {
                continue;
            }
            const QByteArray key = line.left(colon);
            const QByteArray value = line.mid(colon + 1).trimmed();

            if (key == "drm-pdev") {
                device = value;
            } else if (key == "drm-client-id") {
                clientId = value;
            } else if (key.startsWith("drm-engine-") && value.endsWith("ns")) {
                bool ok = false;
                const unsigned long long ns = value.left(value.size() - 2).trimmed().toULongLong(&ok);
                if (ok) {
                    engines.append({key, ns});
                }
            } else if (key.startsWith("drm-resident-")) {
                qint64 bytes = 0;
                if (parseDrmSize(value, bytes)) {
                    residentBytes = qMax(residentBytes, qint64(0)) + bytes;
                }
            } else if (key.startsWith("drm-memory-")) {
                qint64 bytes = 0;
                if (parseDrmSize(value, bytes)) {
                    legacyBytes = qMax(legacyBytes, qint64(0)) + bytes;
                }
            }
        }

        const QByteArray clientKey = device + '|' + clientId;
        for (const auto &engine : engines) {
            currentEngineTime.insert(device + '|' + engine.first + '|' + clientId, engine.second);
        }
        const qint64 clientBytes = residentBytes >= 0 ? residentBytes : legacyBytes;
        if (clientBytes >= 0) {
            clientMemory.insert(clientKey, clientBytes);
        }
    }

    for (const qint64 bytes : std::as_const(clientMemory)) {
        memoryBytes = qMax(memoryBytes, qint64(0)) + bytes;
    }

    const bool hasBaseline = m_drmTimer.isValid();
    const qint64 elapsedNs = hasBaseline ? m_drmTimer.nsecsElapsed() : 0;
    m_drmTimer.start();

    if (!currentEngineTime.isEmpty()) {
        usage = 0.0;
    }
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
}
#endif

void SystemStats::updateGpuStats() {
    double usage = -1.0;
    qint64 memoryBytes = -1;

#ifdef Q_OS_LINUX
    updateDrmStats(usage, memoryBytes);
#endif

    if (m_nvml) {
        const auto pid = static_cast<unsigned int>(QCoreApplication::applicationPid());
        usage = qMax(usage, m_nvml->processUsage(pid));
        const qint64 nvmlBytes = m_nvml->processMemory(pid);
        if (nvmlBytes >= 0) {
            memoryBytes = qMax(memoryBytes, qint64(0)) + nvmlBytes;
        }
    }

    m_gpuPercent = usage;
    m_gpuUsage = usage >= 0.0 ? QString::number(usage, 'f', 1) + "%" : "N/A";
    m_gpuMemory = memoryBytes >= 0 ? formatBytes(memoryBytes) : "N/A";
}

void SystemStats::updateThreadStats() {
    int count = -1;

#if defined(Q_OS_LINUX)
    for (const QByteArray &line : readProcFile("/proc/self/status").split('\n')) {
        if (line.startsWith("Threads:")) {
            bool ok = false;
            const int threads = line.mid(8).trimmed().toInt(&ok);
            if (ok) {
                count = threads;
            }
            break;
        }
    }
#elif defined(Q_OS_WIN)
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        THREADENTRY32 entry;
        entry.dwSize = sizeof(entry);
        const DWORD pid = GetCurrentProcessId();
        if (Thread32First(snapshot, &entry)) {
            count = 0;
            do {
                if (entry.th32OwnerProcessID == pid) {
                    ++count;
                }
            } while (Thread32Next(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
#elif defined(Q_OS_MAC)
    thread_act_array_t threads = nullptr;
    mach_msg_type_number_t threadCount = 0;
    if (task_threads(mach_task_self(), &threads, &threadCount) == KERN_SUCCESS) {
        count = static_cast<int>(threadCount);
        for (mach_msg_type_number_t i = 0; i < threadCount; ++i) {
            mach_port_deallocate(mach_task_self(), threads[i]);
        }
        vm_deallocate(mach_task_self(), static_cast<vm_address_t>(reinterpret_cast<uintptr_t>(threads)), sizeof(thread_t) * threadCount);
    }
#endif

    m_threadCount = count >= 0 ? QString::number(count) : "N/A";
}

void SystemStats::updateStats() {
    updateCpuStats();
    updateRamStats();
    updateIoStats();
    updateGpuStats();
    updateThreadStats();

    emit statsUpdated();
}
