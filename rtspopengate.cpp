#include "rtspopengate.h"

RtspOpenGate::RtspOpenGate(QObject *parent) : QObject(parent) {}

RtspOpenGate &RtspOpenGate::instance() {
    static RtspOpenGate gate;
    return gate;
}

bool RtspOpenGate::request(int id) {
    if (m_holder == id) {
        return true;
    }
    if (m_holder < 0) {
        m_holder = id;
        m_overlapped = m_otherOpens > 0;
        return true;
    }
    if (!m_waiting.contains(id)) {
        m_waiting.append(id);
    }
    return false;
}

void RtspOpenGate::release(int id) {
    if (m_holder != id) {
        return;
    }
    m_holder = -1;
    m_overlapped = false;
    admitNext();
}

void RtspOpenGate::cancel(int id) {
    if (m_holder == id) {
        release(id);
        return;
    }
    m_waiting.removeAll(id);
}

void RtspOpenGate::beginOtherOpen() {
    ++m_otherOpens;
    if (m_holder >= 0) {
        m_overlapped = true;
    }
}

void RtspOpenGate::endOtherOpen() {
    if (m_otherOpens > 0) {
        --m_otherOpens;
    }
}

bool RtspOpenGate::holds(int id) const { return m_holder == id; }

bool RtspOpenGate::waiting(int id) const { return m_waiting.contains(id); }

bool RtspOpenGate::overlapped(int id) const { return m_holder == id && m_overlapped; }

void RtspOpenGate::admitNext() {
    if (m_waiting.isEmpty()) {
        return;
    }
    m_holder = m_waiting.takeFirst();
    m_overlapped = m_otherOpens > 0;
    emit admitted(m_holder);
}
