#include "rtspconnection.h"

#include "streamaddress.h"

#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>

#include <algorithm>
#include <utility>

namespace {

QMutex openFailureMutex;
QString openFailureDescription;

} // namespace

RtspConnection::Step RtspConnection::open(const QUrl &address, Setting setting, qint64 nowMs) {
    close();
    m_sourceKey = StreamAddress::sourceKey(address);
    const bool secure = StreamAddress::isSecureRtsp(address);
    m_forced = !secure && setting != Setting::Automatic;
    if (secure || setting == Setting::Tcp || (setting == Setting::Automatic && m_needsTcp.contains(m_sourceKey))) {
        m_attempts = {Transport::Tcp};
    } else if (setting == Setting::Udp) {
        m_attempts = {Transport::Udp};
    } else {
        m_attempts = {Transport::Udp, Transport::Tcp};
    }
    m_fallsBack = m_attempts.size() > 1;
    m_attempt = 0;
    m_state = State::Connecting;
    return startAttempt(nowMs);
}

RtspConnection::Step RtspConnection::firstPicture() {
    if (m_state == State::Connecting) {
        m_transportInUse = m_attempts[m_attempt];
        if (m_fallsBack && m_transportInUse == Transport::Tcp) {
            m_needsTcp.insert(m_sourceKey);
        }
        m_state = State::Playing;
    } else if (m_state == State::Reconnecting && m_reconnectAttemptRunning) {
        m_reconnectAttemptRunning = false;
        m_reconnectAttempt = 0;
        m_state = State::Playing;
    }
    return {};
}

RtspConnection::Step RtspConnection::playerError(Failure failure, qint64 nowMs, bool paused) {
    switch (m_state) {
    case State::Connecting:
        return attemptFailed(failure, nowMs);
    case State::Playing:
        return lost(nowMs, paused);
    case State::Reconnecting:
        return m_reconnectAttemptRunning ? reconnectAttemptFailed(failure, nowMs) : Step{};
    default:
        return {};
    }
}

RtspConnection::Step RtspConnection::ended(bool hadDuration, qint64 nowMs, bool paused) {
    if (m_state != State::Playing) {
        return {};
    }
    if (hadDuration) {
        return {Step::Kind::Ended};
    }
    return lost(nowMs, paused);
}

RtspConnection::Step RtspConnection::lost(qint64 nowMs, bool paused) {
    if (m_state != State::Playing) {
        return {};
    }
    if (paused) {
        close();
        m_reopenOnPlay = true;
        return {};
    }
    return startReconnecting(nowMs);
}

RtspConnection::Step RtspConnection::timePassed(qint64 nowMs) {
    const qint64 deadline = deadlineMs();
    if (deadline < 0 || nowMs < deadline) {
        return {};
    }
    if (m_state == State::Connecting) {
        return attemptFailed(Failure::Other, nowMs);
    }
    if (m_reconnectAttemptRunning) {
        return reconnectAttemptFailed(Failure::Other, nowMs);
    }
    m_reconnectAttemptRunning = true;
    m_attemptStartMs = nowMs;
    return {Step::Kind::Open, m_transportInUse, networkTimeoutMs};
}

void RtspConnection::close() {
    m_state = State::Idle;
    m_attempts.clear();
    m_attempt = 0;
    m_transportInUse = Transport::None;
    m_reconnectAttempt = 0;
    m_reconnectAttemptRunning = false;
    m_reopenOnPlay = false;
}

void RtspConnection::forgetTransports() { m_needsTcp.clear(); }

RtspConnection::State RtspConnection::state() const { return m_state; }

RtspConnection::Transport RtspConnection::transportInUse() const { return m_transportInUse; }

qint64 RtspConnection::deadlineMs() const {
    if (m_state == State::Connecting) {
        return m_attemptStartMs + (isLastAttempt() ? lastAttemptLimitMs : fallbackAfterMs);
    }
    if (m_state == State::Reconnecting) {
        return m_reconnectAttemptRunning ? m_attemptStartMs + lastAttemptLimitMs : m_nextReconnectMs;
    }
    return -1;
}

bool RtspConnection::reopenOnPlay() const { return m_reopenOnPlay; }

bool RtspConnection::opening() const { return m_state == State::Connecting || (m_state == State::Reconnecting && m_reconnectAttemptRunning); }

int RtspConnection::reconnectAttempt() const { return m_reconnectAttempt; }

RtspConnection::Setting RtspConnection::settingFor(const QString &value) {
    if (value == QLatin1String("tcp")) {
        return Setting::Tcp;
    }
    if (value == QLatin1String("udp")) {
        return Setting::Udp;
    }
    return Setting::Automatic;
}

QString RtspConnection::settingName(Setting setting) {
    switch (setting) {
    case Setting::Tcp:
        return QStringLiteral("tcp");
    case Setting::Udp:
        return QStringLiteral("udp");
    default:
        return QStringLiteral("auto");
    }
}

QString RtspConnection::transportName(Transport transport) {
    switch (transport) {
    case Transport::Tcp:
        return QStringLiteral("TCP");
    case Transport::Udp:
        return QStringLiteral("UDP");
    default:
        return {};
    }
}

RtspConnection::Failure RtspConnection::failureFor(const QString &description) {
    static const QRegularExpression signIn(QStringLiteral(R"(\b(401|403)\b)"));
    static const QRegularExpression notFound(QStringLiteral(R"(\b(404|400)\b)"));
    if (signIn.match(description).hasMatch()) {
        return Failure::SignIn;
    }
    if (notFound.match(description).hasMatch()) {
        return Failure::NotFound;
    }
    return Failure::Other;
}

void RtspConnection::noteOpenFailure(const QString &description) {
    const QMutexLocker locker(&openFailureMutex);
    openFailureDescription = description;
}

QString RtspConnection::takeOpenFailure() {
    const QMutexLocker locker(&openFailureMutex);
    return std::exchange(openFailureDescription, QString());
}

RtspConnection::Step RtspConnection::startAttempt(qint64 nowMs) {
    m_attemptStartMs = nowMs;
    return {Step::Kind::Open, m_attempts[m_attempt], networkTimeoutMs};
}

RtspConnection::Step RtspConnection::attemptFailed(Failure failure, qint64 nowMs) {
    if (failure != Failure::SignIn && failure != Failure::NotFound && !isLastAttempt()) {
        ++m_attempt;
        return startAttempt(nowMs);
    }
    return fail(failure == Failure::None ? Failure::Other : failure);
}

RtspConnection::Step RtspConnection::fail(Failure failure) {
    const Transport tried = m_state == State::Connecting ? m_attempts[m_attempt] : m_transportInUse;
    const bool forced = m_forced;
    close();
    m_state = State::Failed;
    return {Step::Kind::Fail, tried, 0, failure, forced};
}

RtspConnection::Step RtspConnection::startReconnecting(qint64 nowMs) {
    m_state = State::Reconnecting;
    m_lossMs = nowMs;
    m_reconnectAttempt = 1;
    m_reconnectAttemptRunning = false;
    m_nextReconnectMs = nowMs + reconnectStartsMs[0];
    return {Step::Kind::Wait, m_transportInUse};
}

RtspConnection::Step RtspConnection::reconnectAttemptFailed(Failure failure, qint64 nowMs) {
    m_reconnectAttemptRunning = false;
    if (failure == Failure::SignIn) {
        return fail(Failure::SignIn);
    }
    if (m_reconnectAttempt >= maxReconnectAttempts) {
        return fail(Failure::Lost);
    }
    m_nextReconnectMs = std::max(nowMs, m_lossMs + reconnectStartsMs[m_reconnectAttempt]);
    ++m_reconnectAttempt;
    return {Step::Kind::Wait, m_transportInUse};
}

bool RtspConnection::isLastAttempt() const { return m_attempt + 1 >= m_attempts.size(); }
