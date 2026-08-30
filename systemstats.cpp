#include "systemstats.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

SystemStats::SystemStats(QObject *parent) : QObject(parent) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SystemStats::updateStats);
    m_timer->start(1000); // Update every 1 second
    updateStats(); // Initial update
}

QString SystemStats::cpuUsage() const { return m_cpuUsage; }
QString SystemStats::ramUsage() const { return m_ramUsage; }
QString SystemStats::ioUsage() const { return m_ioUsage; }
QString SystemStats::gpuUsage() const { return m_gpuUsage; }

void SystemStats::updateStats() {
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

    emit statsUpdated();
}
