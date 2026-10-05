#include "shuffleorder.h"

#include <QSet>

#include <algorithm>
#include <utility>

ShuffleOrder::ShuffleOrder(QObject *parent) : QObject(parent), m_random(QRandomGenerator::securelySeeded()) {}

bool ShuffleOrder::enabled() const { return m_enabled; }

void ShuffleOrder::setEnabled(bool enabled) {
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    emit enabledChanged();
}

int ShuffleOrder::remaining() const { return int(m_remaining.size()); }

bool ShuffleOrder::canGoBack() const { return m_history.size() >= 2; }

int ShuffleOrder::current() const { return m_history.isEmpty() ? -1 : m_history.last(); }

void ShuffleOrder::setSeed(quint32 seed) { m_random.seed(seed); }

void ShuffleOrder::refill(int exclude) {
    m_remaining.clear();
    for (const int id : std::as_const(m_candidates)) {
        if (id != exclude) {
            m_remaining.append(id);
        }
    }
    for (qsizetype i = m_remaining.size() - 1; i > 0; --i) {
        const qsizetype j = qsizetype(m_random.bounded(quint32(i + 1)));
        std::swap(m_remaining[i], m_remaining[j]);
    }
}

void ShuffleOrder::reset(const QList<int> &ids, int currentId) {
    m_candidates = ids;
    m_history.clear();
    const int current = m_candidates.contains(currentId) ? currentId : -1;
    if (current >= 0) {
        m_history.append(current);
    }
    refill(current);
    emit orderChanged();
}

void ShuffleOrder::setCandidates(const QList<int> &ids) {
    const QSet<int> before(m_candidates.cbegin(), m_candidates.cend());
    const QSet<int> after(ids.cbegin(), ids.cend());
    m_candidates = ids;

    const auto gone = [&after](int id) { return !after.contains(id); };
    m_remaining.removeIf(gone);
    m_history.removeIf(gone);
    m_history.erase(std::unique(m_history.begin(), m_history.end()), m_history.end());

    for (const int id : ids) {
        if (!before.contains(id)) {
            m_remaining.insert(qsizetype(m_random.bounded(quint32(m_remaining.size() + 1))), id);
        }
    }
    emit orderChanged();
}

int ShuffleOrder::next(bool repeatPlaylist) {
    if (m_remaining.isEmpty()) {
        if (!repeatPlaylist || m_candidates.isEmpty()) {
            return -1;
        }
        refill(m_candidates.size() > 1 ? current() : -1);
        if (m_remaining.isEmpty()) {
            return -1;
        }
    }
    const int id = m_remaining.takeFirst();
    m_history.append(id);
    emit orderChanged();
    return id;
}

int ShuffleOrder::previous() {
    if (m_history.size() < 2) {
        return -1;
    }
    m_remaining.prepend(m_history.takeLast());
    emit orderChanged();
    return m_history.last();
}

void ShuffleOrder::setCurrent(int id) {
    if (!m_candidates.contains(id) || current() == id) {
        return;
    }
    m_remaining.removeAll(id);
    m_history.append(id);
    emit orderChanged();
}
