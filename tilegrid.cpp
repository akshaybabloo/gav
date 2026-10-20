#include "tilegrid.h"

#include <algorithm>

namespace {

QVariantMap handled(QVariantMap result = {}) {
    result.insert(QStringLiteral("handled"), true);
    return result;
}

QVariantMap refused() { return {{QStringLiteral("refused"), true}}; }

QVariant slotList(QList<int> list) {
    std::sort(list.begin(), list.end());
    return QVariant::fromValue(list);
}

} // namespace

TileGrid::TileGrid(QObject *parent) : QObject(parent), m_entries(maxTiles, -1) {}

int TileGrid::count() const { return int(m_order.size()); }

bool TileGrid::splitView() const { return m_order.size() > 1; }

int TileGrid::selectedSlot() const { return m_selected; }

bool TileGrid::focused() const { return m_focused >= 0; }

int TileGrid::focusedSlot() const { return m_focused; }

QList<int> TileGrid::entryIds() const { return m_entries; }

int TileGrid::revision() const { return m_revision; }

void TileGrid::notifyChanged() {
    ++m_revision;
    emit changed();
}

int TileGrid::positionOf(int slot) const { return int(m_order.indexOf(slot)) + 1; }

int TileGrid::slotAtPosition(int position) const {
    if (position < 1 || position > m_order.size()) {
        return -1;
    }
    return m_order.at(position - 1);
}

int TileGrid::slotOf(int entryId) const {
    if (entryId < 0) {
        return -1;
    }
    return int(m_entries.indexOf(entryId));
}

QRectF TileGrid::cellAtPosition(int position, qreal width, qreal height) const {
    const qsizetype tiles = m_order.size();
    if (tiles <= 1) {
        return {0, 0, width, height};
    }
    const int index = position - 1;
    if (tiles == 2) {
        return {index * width / 2, 0, width / 2, height};
    }
    const int column = index % 2;
    const int row = index / 2;
    return {column * width / 2, row * height / 2, width / 2, height / 2};
}

QRectF TileGrid::cellFor(int slot, qreal width, qreal height) const {
    if (m_order.isEmpty() && slot == m_selected) {
        return {0, 0, width, height};
    }
    const int position = positionOf(slot);
    if (position == 0) {
        return {};
    }
    if (m_focused >= 0) {
        return slot == m_focused ? QRectF(0, 0, width, height) : QRectF();
    }
    return cellAtPosition(position, width, height);
}

int TileGrid::slotAt(qreal x, qreal y, qreal width, qreal height) const {
    for (const int slot : m_order) {
        const QRectF cell = cellFor(slot, width, height);
        if (cell.contains(x, y)) {
            return slot;
        }
    }
    return -1;
}

QList<int> TileGrid::newEntries(const QList<int> &entryIds) const {
    QList<int> result;
    for (const int id : entryIds) {
        if (id >= 0 && !m_entries.contains(id) && !result.contains(id)) {
            result.append(id);
        }
    }
    return result;
}

int TileGrid::resultingCount(const QList<int> &entryIds) const { return count() + int(newEntries(entryIds).size()); }

bool TileGrid::canAdd(const QList<int> &entryIds) const {
    const int result = resultingCount(entryIds);
    return result > count() && result >= 2 && result <= maxTiles;
}

int TileGrid::freeSlot() const { return int(m_entries.indexOf(-1)); }

QList<int> TileGrid::fill(const QList<int> &entryIds) {
    QList<int> filled;
    for (const int id : entryIds) {
        const int slot = freeSlot();
        m_entries[slot] = id;
        m_order.append(slot);
        filled.append(slot);
    }
    return filled;
}

QList<int> TileGrid::leaveFocus() {
    QList<int> resume(m_pausedByFocus.begin(), m_pausedByFocus.end());
    m_pausedByFocus.clear();
    m_focused = -1;
    return resume;
}

QVariantMap TileGrid::open(int entryId) {
    if (count() >= 2) {
        return replace(m_selected, entryId);
    }
    if (count() == 1) {
        return replace(m_order.first(), entryId);
    }
    if (entryId < 0) {
        return refused();
    }
    const QList<int> filled = fill({entryId});
    m_selected = filled.first();
    notifyChanged();
    return {{QStringLiteral("load"), slotList(filled)}};
}

QVariantMap TileGrid::start(const QList<int> &entryIds) {
    const QList<int> entries = newEntries(entryIds);
    if (count() != 0 || entries.size() < 2 || entries.size() > maxTiles || entries.size() != entryIds.size()) {
        return refused();
    }
    const QList<int> filled = fill(entries);
    m_selected = filled.first();
    notifyChanged();
    return {{QStringLiteral("load"), slotList(filled)}};
}

QVariantMap TileGrid::add(const QList<int> &entryIds) {
    if (count() == 0) {
        return start(entryIds);
    }
    if (!canAdd(entryIds)) {
        return refused();
    }
    const QList<int> resume = leaveFocus();
    const QList<int> filled = fill(newEntries(entryIds));
    notifyChanged();
    return {{QStringLiteral("load"), slotList(filled)}, {QStringLiteral("resume"), slotList(resume)}};
}

QVariantMap TileGrid::replace(int slot, int entryId) {
    if (positionOf(slot) == 0 || entryId < 0) {
        return refused();
    }
    const int existing = slotOf(entryId);
    if (existing >= 0) {
        select(existing);
        return {{QStringLiteral("load"), slotList({})}};
    }
    m_entries[slot] = entryId;
    notifyChanged();
    return {{QStringLiteral("load"), slotList({slot})}};
}

QVariantMap TileGrid::close(int slot) {
    if (positionOf(slot) == 0) {
        return {{QStringLiteral("closed"), slotList({})}, {QStringLiteral("resume"), slotList({})}};
    }
    QList<int> resume = leaveFocus();
    resume.removeAll(slot);
    m_entries[slot] = -1;
    m_order.removeAll(slot);
    if (!m_order.isEmpty()) {
        m_selected = m_order.first();
    }
    notifyChanged();
    return {{QStringLiteral("closed"), slotList({slot})}, {QStringLiteral("resume"), slotList(resume)}};
}

QVariantMap TileGrid::end() {
    QList<int> closed;
    for (const int slot : std::as_const(m_order)) {
        if (slot != m_selected) {
            closed.append(slot);
        }
    }
    if (closed.isEmpty() && m_focused < 0) {
        return {{QStringLiteral("closed"), slotList({})}, {QStringLiteral("resume"), slotList({})}};
    }
    QList<int> resume = leaveFocus();
    resume.removeIf([this](int slot) { return slot != m_selected; });
    for (const int slot : std::as_const(closed)) {
        m_entries[slot] = -1;
        m_order.removeAll(slot);
    }
    notifyChanged();
    return {{QStringLiteral("closed"), slotList(closed)}, {QStringLiteral("resume"), slotList(resume)}};
}

QVariantMap TileGrid::entryRemoved(int entryId) { return close(slotOf(entryId)); }

void TileGrid::clear() {
    if (m_order.isEmpty()) {
        return;
    }
    m_entries.fill(-1);
    m_order.clear();
    leaveFocus();
    notifyChanged();
}

void TileGrid::select(int slot) {
    if (positionOf(slot) == 0 || slot == m_selected) {
        return;
    }
    m_selected = slot;
    notifyChanged();
}

QVariantMap TileGrid::pressPosition(int position, const QList<int> &playingFiles) {
    if (!splitView()) {
        return {};
    }
    const int slot = slotAtPosition(position);
    if (slot < 0) {
        return handled();
    }
    if (m_focused < 0) {
        select(slot);
        return handled();
    }
    if (slot == m_focused) {
        return handled();
    }
    QList<int> pause;
    if (playingFiles.contains(m_focused)) {
        pause.append(m_focused);
        m_pausedByFocus.insert(m_focused);
    }
    QList<int> resume;
    if (m_pausedByFocus.remove(slot)) {
        resume.append(slot);
    }
    m_focused = slot;
    m_selected = slot;
    notifyChanged();
    return handled({{QStringLiteral("pause"), slotList(pause)}, {QStringLiteral("resume"), slotList(resume)}});
}

QVariantMap TileGrid::doubleClick(int slot, bool fullScreen, const QList<int> &playingFiles) {
    if (!splitView() || positionOf(slot) == 0) {
        return {};
    }
    if (m_focused < 0) {
        QList<int> pause;
        for (const int other : std::as_const(m_order)) {
            if (other != slot && playingFiles.contains(other)) {
                pause.append(other);
                m_pausedByFocus.insert(other);
            }
        }
        m_focused = slot;
        m_selected = slot;
        notifyChanged();
        return handled({{QStringLiteral("pause"), slotList(pause)}});
    }
    if (!fullScreen) {
        return handled({{QStringLiteral("enterFullScreen"), true}});
    }
    const QList<int> resume = leaveFocus();
    notifyChanged();
    return handled({{QStringLiteral("leaveFullScreen"), true}, {QStringLiteral("resume"), slotList(resume)}});
}

QVariantMap TileGrid::escape(bool fullScreen) {
    if (m_focused < 0) {
        return {};
    }
    if (fullScreen) {
        return handled({{QStringLiteral("leaveFullScreen"), true}, {QStringLiteral("resume"), slotList({})}});
    }
    const QList<int> resume = leaveFocus();
    notifyChanged();
    return handled({{QStringLiteral("resume"), slotList(resume)}});
}
