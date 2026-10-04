#include "shuffleorder.h"

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
    for (int i = 0; i < m_count; ++i) {
        if (i != exclude) {
            m_remaining.append(i);
        }
    }
    for (qsizetype i = m_remaining.size() - 1; i > 0; --i) {
        const qsizetype j = qsizetype(m_random.bounded(quint32(i + 1)));
        std::swap(m_remaining[i], m_remaining[j]);
    }
}

void ShuffleOrder::reset(int count, int currentIndex) {
    m_count = qMax(0, count);
    m_history.clear();
    const int current = currentIndex >= 0 && currentIndex < m_count ? currentIndex : -1;
    if (current >= 0) {
        m_history.append(current);
    }
    refill(current);
    emit orderChanged();
}

int ShuffleOrder::next(bool repeatPlaylist) {
    if (m_remaining.isEmpty()) {
        if (!repeatPlaylist || m_count == 0) {
            return -1;
        }
        refill(m_count > 1 ? current() : -1);
        if (m_remaining.isEmpty()) {
            return -1;
        }
    }
    const int index = m_remaining.takeFirst();
    m_history.append(index);
    emit orderChanged();
    return index;
}

int ShuffleOrder::previous() {
    if (m_history.size() < 2) {
        return -1;
    }
    m_remaining.prepend(m_history.takeLast());
    emit orderChanged();
    return m_history.last();
}

void ShuffleOrder::setCurrent(int index) {
    if (index < 0 || index >= m_count || current() == index) {
        return;
    }
    m_remaining.removeAll(index);
    m_history.append(index);
    emit orderChanged();
}

void ShuffleOrder::itemInserted(int index) {
    if (index < 0 || index > m_count) {
        return;
    }
    ++m_count;
    for (int &value : m_remaining) {
        if (value >= index) {
            ++value;
        }
    }
    for (int &value : m_history) {
        if (value >= index) {
            ++value;
        }
    }
    m_remaining.insert(qsizetype(m_random.bounded(quint32(m_remaining.size() + 1))), index);
    emit orderChanged();
}

void ShuffleOrder::itemRemoved(int index) {
    if (index < 0 || index >= m_count) {
        return;
    }
    --m_count;
    m_remaining.removeAll(index);
    m_history.removeAll(index);
    for (int &value : m_remaining) {
        if (value > index) {
            --value;
        }
    }
    for (int &value : m_history) {
        if (value > index) {
            --value;
        }
    }
    m_history.erase(std::unique(m_history.begin(), m_history.end()), m_history.end());
    emit orderChanged();
}

int ShuffleOrder::remap(int value, int from, int to) {
    if (value == from) {
        return to;
    }
    if (from < to && value > from && value <= to) {
        return value - 1;
    }
    if (from > to && value >= to && value < from) {
        return value + 1;
    }
    return value;
}

void ShuffleOrder::itemMoved(int from, int to) {
    if (from < 0 || from >= m_count || to < 0 || to >= m_count || from == to) {
        return;
    }
    for (int &value : m_remaining) {
        value = remap(value, from, to);
    }
    for (int &value : m_history) {
        value = remap(value, from, to);
    }
    emit orderChanged();
}
