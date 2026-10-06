#include "playlistview.h"

#include <QCollator>
#include <QHash>
#include <QLocale>

#include <algorithm>
#include <limits>
#include <numeric>
#include <vector>

PlaylistView::PlaylistView(QObject *parent) : QAbstractListModel(parent) {}

int PlaylistView::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : int(m_rows.size()); }

QVariant PlaylistView::data(const QModelIndex &index, int role) const {
    if (!m_source || !index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const Row &row = m_rows[index.row()];
    if (row.sourceRow < 0) {
        if (row.sourceRow != -1 || row.group < 0 || row.group >= m_groups.size()) {
            return {};
        }
        const Group &group = m_groups[row.group];
        switch (role) {
        case IsHeaderRole:
            return true;
        case Qt::DisplayRole:
        case PlaylistModel::TitleRole:
        case PlaylistModel::GroupRole:
            return group.name;
        case GroupCountRole:
            return group.count;
        case CollapsedRole:
            return group.collapsed;
        case SelectedRole:
            return false;
        case SourceRowRole:
        case PlaylistModel::EntryIdRole:
            return -1;
        default:
            return {};
        }
    }
    switch (role) {
    case IsHeaderRole:
    case SelectedRole:
    case CollapsedRole:
        return false;
    case GroupCountRole:
        return 0;
    case SourceRowRole:
        return row.sourceRow;
    default:
        return m_source->data(m_source->index(row.sourceRow), role);
    }
}

QHash<int, QByteArray> PlaylistView::roleNames() const {
    QHash<int, QByteArray> names = m_source ? m_source->roleNames() : PlaylistModel().roleNames();
    names.insert(IsHeaderRole, "isHeader");
    names.insert(SelectedRole, "selected");
    names.insert(SourceRowRole, "sourceRow");
    names.insert(GroupCountRole, "groupCount");
    names.insert(CollapsedRole, "collapsed");
    return names;
}

PlaylistModel *PlaylistView::source() const { return m_source; }

void PlaylistView::setSource(PlaylistModel *source) {
    if (m_source == source) {
        return;
    }
    if (m_source) {
        m_source->disconnect(this);
    }
    const State before = state();
    beginResetModel();
    m_source = source;
    refold();
    assign(build());
    endResetModel();

    if (m_source) {
        connect(m_source, &QAbstractItemModel::rowsAboutToBeInserted, this, [this](const QModelIndex &, int first, int last) {
            m_before = state();
            m_forwarding = canReorder() && !m_grouped;
            if (m_forwarding) {
                beginInsertRows(QModelIndex(), first, last);
            } else {
                beginResetModel();
            }
        });
        connect(m_source, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &, int first, int last) {
            for (int row = first; row <= last; ++row) {
                const Folded folded = fold(row);
                m_entriesWithGroup += folded.group.isEmpty() ? 0 : 1;
                m_folded.insert(row, folded);
            }
            assign(build());
            if (m_forwarding) {
                endInsertRows();
            } else {
                endResetModel();
            }
            const int added = last - first + 1;
            const int hidden = added - (m_matchCount - m_before.matchCount);
            notify(m_before);
            if (hidden > 0) {
                emit addedEntriesHidden(added, hidden);
            }
        });
        connect(m_source, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this](const QModelIndex &, int first, int last) {
            m_before = state();
            m_forwarding = canReorder() && !m_grouped;
            if (m_forwarding) {
                beginRemoveRows(QModelIndex(), first, last);
            }
        });
        connect(m_source, &QAbstractItemModel::rowsRemoved, this, [this](const QModelIndex &, int first, int last) {
            const int removed = last - first + 1;
            for (int row = first; row <= last; ++row) {
                m_entriesWithGroup -= m_folded[row].group.isEmpty() ? 0 : 1;
            }
            m_folded.remove(first, removed);
            if (m_forwarding) {
                assign(build());
                endRemoveRows();
            } else {
                for (Row &row : m_rows) {
                    if (row.sourceRow > last) {
                        row.sourceRow -= removed;
                    } else if (row.sourceRow >= first) {
                        row.sourceRow = std::numeric_limits<int>::min();
                    }
                }
                removeHiddenRows();
            }
            notify(m_before);
        });
        connect(m_source, &QAbstractItemModel::modelAboutToBeReset, this, [this] {
            m_before = state();
            beginResetModel();
        });
        connect(m_source, &QAbstractItemModel::modelReset, this, [this] {
            refold();
            assign(build());
            endResetModel();
            notify(m_before);
        });
        connect(m_source, &QAbstractItemModel::dataChanged, this,
                [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles) {
                    for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
                        const int viewRow = viewRowFor(row);
                        if (viewRow >= 0) {
                            emit dataChanged(index(viewRow), index(viewRow), roles);
                        }
                    }
                    if (roles.isEmpty() || roles.contains(PlaylistModel::AvailableRole)) {
                        emit playableEntriesChanged();
                        updateNavigation();
                    }
                });
        connect(m_source, &PlaylistModel::currentRowChanged, this, [this] {
            updateCurrent();
            updateNavigation();
        });
    }
    emit sourceChanged();
    notify(before);
}

QString PlaylistView::searchText() const { return m_searchText; }

void PlaylistView::setSearchText(const QString &text) {
    if (text == m_searchText) {
        return;
    }
    const State before = state();
    m_searchText = text;
    const QString needle = text.trimmed().toCaseFolded();
    if (needle != m_needle) {
        m_needle = needle;
        resetLayout(before);
    }
    emit searchTextChanged();
}

PlaylistView::Filter PlaylistView::filter() const { return m_filter; }

void PlaylistView::setFilter(Filter filter) {
    if (filter == m_filter) {
        return;
    }
    const State before = state();
    m_filter = filter;
    resetLayout(before);
    emit filterChanged();
}

PlaylistView::SortOrder PlaylistView::sortOrder() const { return m_sortOrder; }

void PlaylistView::setSortOrder(SortOrder order) {
    if (order == m_sortOrder) {
        return;
    }
    const State before = state();
    m_sortOrder = order;
    resetLayout(before);
    emit sortOrderChanged();
}

bool PlaylistView::grouped() const { return m_grouped; }

void PlaylistView::setGrouped(bool grouped) {
    if (grouped == m_grouped) {
        return;
    }
    const State before = state();
    m_grouped = grouped;
    if (hasGroups()) {
        resetLayout(before);
    }
    emit groupedChanged();
}

int PlaylistView::matchCount() const { return m_matchCount; }

bool PlaylistView::hasGroups() const { return m_entriesWithGroup > 0; }

bool PlaylistView::groupingActive() const { return m_grouped && hasGroups(); }

bool PlaylistView::canReorder() const { return m_needle.isEmpty() && m_filter == All && m_sortOrder == PlaylistOrder && !groupingActive(); }

void PlaylistView::toggleGroup(const QString &group) {
    const bool collapse = !m_collapsed.contains(group);
    if (collapse) {
        m_collapsed.insert(group);
    } else {
        m_collapsed.remove(group);
    }
    int header = -1;
    for (int row = 0; row < m_rows.size(); ++row) {
        if (m_rows[row].sourceRow < 0 && m_groups[m_rows[row].group].name == group) {
            header = row;
            break;
        }
    }
    if (header < 0) {
        return;
    }
    const int count = m_groups[m_rows[header].group].count;
    const Layout layout = build();
    if (count > 0) {
        if (collapse) {
            beginRemoveRows(QModelIndex(), header + 1, header + count);
            assign(layout);
            endRemoveRows();
        } else {
            beginInsertRows(QModelIndex(), header + 1, header + count);
            assign(layout);
            endInsertRows();
        }
    } else {
        assign(layout);
    }
    emit dataChanged(index(header), index(header), {CollapsedRole});
    emit visibleEntriesChanged();
    updateCurrent();
    updateNavigation();
}

void PlaylistView::clearSearchAndFilter() {
    const bool hadText = !m_searchText.isEmpty();
    const bool hadFilter = m_filter != All;
    if (!hadText && !hadFilter) {
        return;
    }
    const bool rebuild = !m_needle.isEmpty() || hadFilter;
    const State before = state();
    m_searchText.clear();
    m_needle.clear();
    m_filter = All;
    if (rebuild) {
        resetLayout(before);
    }
    if (hadText) {
        emit searchTextChanged();
    }
    if (hadFilter) {
        emit filterChanged();
    }
}

PlaylistView::Folded PlaylistView::fold(int sourceRow) const {
    const PlaylistModel::Entry *entry = m_source ? m_source->entry(sourceRow) : nullptr;
    return entry ? Folded{entry->title.toCaseFolded(), entry->group.toCaseFolded()} : Folded{};
}

void PlaylistView::refold() {
    m_folded.clear();
    m_entriesWithGroup = 0;
    const int count = m_source ? m_source->count() : 0;
    m_folded.reserve(count);
    for (int row = 0; row < count; ++row) {
        m_folded.append(fold(row));
        m_entriesWithGroup += m_folded.last().group.isEmpty() ? 0 : 1;
    }
}

bool PlaylistView::matches(int sourceRow) const {
    const PlaylistModel::Entry *entry = m_source->entry(sourceRow);
    if (!entry) {
        return false;
    }
    if (m_filter != All && (entry->kind == PlaylistModel::Stream) != (m_filter == Streams)) {
        return false;
    }
    return m_needle.isEmpty() || m_folded[sourceRow].title.contains(m_needle) || m_folded[sourceRow].group.contains(m_needle);
}

PlaylistView::Layout PlaylistView::build() const {
    Layout layout;
    if (!m_source) {
        return layout;
    }
    const int count = m_source->count();
    QList<int> matched;
    matched.reserve(count);
    for (int row = 0; row < count; ++row) {
        if (matches(row)) {
            matched.append(row);
        }
    }
    layout.matchCount = int(matched.size());

    if (m_sortOrder == Title) {
        QLocale locale;
        if (locale.language() == QLocale::C) {
            locale = QLocale(QLocale::English);
        }
        QCollator collator(locale);
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        collator.setNumericMode(true);
        std::vector<QCollatorSortKey> keys;
        keys.reserve(size_t(matched.size()));
        for (const int row : matched) {
            keys.push_back(collator.sortKey(m_source->entry(row)->title));
        }
        std::vector<int> order(size_t(matched.size()));
        std::iota(order.begin(), order.end(), 0);
        std::stable_sort(order.begin(), order.end(), [&keys](int a, int b) { return keys[size_t(a)] < keys[size_t(b)]; });
        QList<int> sorted;
        sorted.reserve(matched.size());
        for (const int position : order) {
            sorted.append(matched[position]);
        }
        matched = sorted;
    } else if (m_sortOrder == Duration) {
        const auto duration = [this](int row) {
            const qint64 value = m_source->entry(row)->durationMs;
            return value > 0 ? value : std::numeric_limits<qint64>::max();
        };
        std::stable_sort(matched.begin(), matched.end(), [&duration](int a, int b) { return duration(a) < duration(b); });
    }

    if (!groupingActive()) {
        layout.rows.reserve(matched.size());
        for (const int row : matched) {
            layout.rows.append({row, -1});
        }
        return layout;
    }

    QHash<QString, int> groupIndex;
    for (int row = 0; row < count; ++row) {
        const QString &name = m_source->entry(row)->group;
        if (!name.isEmpty() && !groupIndex.contains(name)) {
            groupIndex.insert(name, int(layout.groups.size()));
            layout.groups.append({name, 0, m_collapsed.contains(name)});
        }
    }
    const int ungrouped = int(layout.groups.size());
    layout.groups.append({QString(), 0, m_collapsed.contains(QString())});

    QList<QList<int>> members(layout.groups.size());
    for (const int row : matched) {
        const QString &name = m_source->entry(row)->group;
        members[name.isEmpty() ? ungrouped : groupIndex.value(name)].append(row);
    }
    layout.rows.reserve(matched.size() + layout.groups.size());
    for (int group = 0; group < layout.groups.size(); ++group) {
        if (members[group].isEmpty()) {
            continue;
        }
        layout.groups[group].count = int(members[group].size());
        layout.rows.append({-1, group});
        if (!layout.groups[group].collapsed) {
            for (const int row : members[group]) {
                layout.rows.append({row, group});
            }
        }
    }
    return layout;
}

void PlaylistView::assign(const Layout &layout) {
    m_rows = layout.rows;
    m_groups = layout.groups;
    m_matchCount = layout.matchCount;
    m_viewRows.fill(-1, m_source ? m_source->count() : 0);
    for (int row = 0; row < m_rows.size(); ++row) {
        if (m_rows[row].sourceRow >= 0) {
            m_viewRows[m_rows[row].sourceRow] = row;
        }
    }
}

void PlaylistView::resetLayout(const State &before) {
    beginResetModel();
    assign(build());
    endResetModel();
    notify(before);
}

void PlaylistView::removeHiddenRows() {
    const Layout layout = build();
    const auto same = [this, &layout](const Row &current, const Row &target) {
        if (current.sourceRow >= 0 || target.sourceRow >= 0) {
            return current.sourceRow == target.sourceRow;
        }
        return current.sourceRow == -1 && m_groups[current.group].name == layout.groups[target.group].name;
    };
    QList<bool> keep(m_rows.size(), false);
    qsizetype next = 0;
    for (qsizetype row = 0; row < m_rows.size() && next < layout.rows.size(); ++row) {
        if (same(m_rows[row], layout.rows[next])) {
            keep[row] = true;
            ++next;
        }
    }
    if (next != layout.rows.size()) {
        beginResetModel();
        assign(layout);
        endResetModel();
        return;
    }
    for (qsizetype last = m_rows.size() - 1; last >= 0; --last) {
        if (keep[last]) {
            continue;
        }
        qsizetype first = last;
        while (first > 0 && !keep[first - 1]) {
            --first;
        }
        beginRemoveRows(QModelIndex(), int(first), int(last));
        m_rows.remove(first, last - first + 1);
        endRemoveRows();
        last = first;
    }
    QList<int> changedHeaders;
    for (int row = 0; row < m_rows.size(); ++row) {
        if (m_rows[row].sourceRow < 0 && m_groups[m_rows[row].group].count != layout.groups[layout.rows[row].group].count) {
            changedHeaders.append(row);
        }
    }
    assign(layout);
    for (const int row : changedHeaders) {
        emit dataChanged(index(row), index(row), {GroupCountRole});
    }
}

PlaylistView::State PlaylistView::state() const { return {hasGroups(), canReorder(), m_matchCount}; }

void PlaylistView::notify(const State &before) {
    if (before.hasGroups != hasGroups()) {
        emit hasGroupsChanged();
    }
    if (before.canReorder != canReorder()) {
        emit canReorderChanged();
    }
    if (before.matchCount != m_matchCount) {
        emit matchCountChanged();
    }
    emit visibleEntriesChanged();
    updateCurrent();
    updateNavigation();
}

void PlaylistView::updateCurrent() {
    const int row = currentViewRow();
    if (row != m_currentViewRow) {
        m_currentViewRow = row;
        emit currentViewRowChanged();
    }
}

int PlaylistView::currentViewRow() const { return m_source ? viewRowFor(m_source->currentRow()) : -1; }

bool PlaylistView::canGoNext() const { return m_canGoNext; }

bool PlaylistView::canGoPrevious() const { return m_canGoPrevious; }

void PlaylistView::updateNavigation() {
    const bool next = nextRow(false) >= 0;
    const bool previous = previousRow() >= 0;
    if (next == m_canGoNext && previous == m_canGoPrevious) {
        return;
    }
    m_canGoNext = next;
    m_canGoPrevious = previous;
    emit navigationChanged();
}

int PlaylistView::viewRowFor(int sourceRow) const { return sourceRow >= 0 && sourceRow < m_viewRows.size() ? m_viewRows[sourceRow] : -1; }

int PlaylistView::sourceRowFor(int viewRow) const {
    return viewRow >= 0 && viewRow < m_rows.size() && m_rows[viewRow].sourceRow >= 0 ? m_rows[viewRow].sourceRow : -1;
}

bool PlaylistView::available(int viewRow) const {
    const PlaylistModel::Entry *entry = m_source ? m_source->entry(sourceRowFor(viewRow)) : nullptr;
    return entry && entry->availability != PlaylistModel::Unavailable;
}

int PlaylistView::nextRow(bool repeat) const {
    const int count = int(m_rows.size());
    if (!m_source || count == 0) {
        return -1;
    }
    const int current = currentViewRow();
    int start = current + 1;
    if (current < 0) {
        start = 0;
        const int anchor = m_source->currentRow() < 0 ? m_source->anchorRow() : -1;
        if (anchor >= 0) {
            start = count;
            for (int row = 0; row < count; ++row) {
                if (m_rows[row].sourceRow >= anchor) {
                    start = row;
                    break;
                }
            }
        }
    }
    for (int step = 0; step < count; ++step) {
        int row = start + step;
        if (row >= count) {
            if (!repeat) {
                return -1;
            }
            row -= count;
        }
        if (available(row)) {
            return m_rows[row].sourceRow;
        }
    }
    return -1;
}

int PlaylistView::previousRow() const {
    const int count = int(m_rows.size());
    if (!m_source || count == 0) {
        return -1;
    }
    const int current = currentViewRow();
    int start = current - 1;
    if (current < 0) {
        start = -1;
        const int anchor = m_source->currentRow() < 0 ? m_source->anchorRow() : -1;
        for (int row = 0; anchor >= 0 && row < count; ++row) {
            if (m_rows[row].sourceRow >= 0 && m_rows[row].sourceRow < anchor) {
                start = row;
            }
        }
    }
    for (int row = start; row >= 0; --row) {
        if (available(row)) {
            return m_rows[row].sourceRow;
        }
    }
    return -1;
}

QList<int> PlaylistView::playableIds() const {
    QList<int> ids;
    for (int row = 0; row < m_rows.size(); ++row) {
        if (available(row)) {
            ids.append(m_source->idAt(m_rows[row].sourceRow));
        }
    }
    return ids;
}

QList<int> PlaylistView::visibleIds() const {
    QList<int> ids;
    if (!m_source) {
        return ids;
    }
    ids.reserve(m_rows.size());
    for (const Row &row : m_rows) {
        if (row.sourceRow >= 0) {
            ids.append(m_source->idAt(row.sourceRow));
        }
    }
    return ids;
}
