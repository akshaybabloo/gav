#ifndef RTSPCONNECTION_H
#define RTSPCONNECTION_H

#include <QList>
#include <QString>
#include <QUrl>

class RtspConnection {
public:
    enum class Transport { None, Tcp, Udp };
    enum class Setting { Automatic, Tcp, Udp };
    enum class State { Idle, Connecting, Playing, Reconnecting, Failed };
    enum class Failure { None, SignIn, NotFound, Other, Lost };

    struct Step {
        enum class Kind { None, Open, Wait, Fail, Ended };

        Kind kind = Kind::None;
        Transport transport = Transport::None;
        int networkTimeoutMs = 0;
        Failure failure = Failure::None;
        bool forced = false;
    };

    static constexpr int networkTimeoutMs = 5000;
    static constexpr int fallbackAfterMs = 5000;
    static constexpr int lastAttemptLimitMs = 12000;
    static constexpr int maxReconnectAttempts = 3;
    static constexpr int reconnectStartsMs[maxReconnectAttempts] = {2000, 9000, 16000};

    Step open(const QUrl &address, Setting setting, qint64 nowMs);
    Step firstPicture();
    Step playerError(Failure failure, qint64 nowMs, bool paused = false);
    Step ended(bool hadDuration, qint64 nowMs, bool paused = false);
    Step lost(qint64 nowMs, bool paused = false);
    Step timePassed(qint64 nowMs);
    void attemptStarted(qint64 nowMs);
    void close();
    void forgetTransports();
    static void resetTransportMemory();

    State state() const;
    Transport transportInUse() const;
    qint64 deadlineMs() const;
    bool reopenOnPlay() const;
    bool opening() const;
    int reconnectAttempt() const;

    static Setting settingFor(const QString &value);
    static QString settingName(Setting setting);
    static QString transportName(Transport transport);
    static Failure failureFor(const QString &description);
    static void noteOpenFailure(const QString &description);
    static QString takeOpenFailure();

private:
    Step startAttempt(qint64 nowMs);
    Step attemptFailed(Failure failure, qint64 nowMs);
    Step fail(Failure failure);
    Step startReconnecting(qint64 nowMs);
    Step reconnectAttemptFailed(Failure failure, qint64 nowMs);
    bool isLastAttempt() const;

    State m_state = State::Idle;
    QString m_sourceKey;
    QList<Transport> m_attempts;
    qsizetype m_attempt = 0;
    qint64 m_attemptStartMs = 0;
    bool m_forced = false;
    bool m_fallsBack = false;
    Transport m_transportInUse = Transport::None;

    qint64 m_lossMs = 0;
    int m_reconnectAttempt = 0;
    bool m_reconnectAttemptRunning = false;
    qint64 m_nextReconnectMs = 0;
    bool m_reopenOnPlay = false;
};

#endif // RTSPCONNECTION_H
