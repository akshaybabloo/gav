#include "playlistview.h"

PlaylistView::PlaylistView(QObject *parent) : QAbstractListModel(parent) {}

int PlaylistView::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : int(m_sourceRows.size()); }

QVariant PlaylistView::data(const QModelIndex &index, int role) const {
    if (!m_source || !index.isValid() || index.row() < 0 || index.row() >= m_sourceRows.size()) {
        return {};
    }
    const int sourceRow = m_sourceRows[index.row()];
    switch (role) {
    case IsHeaderRole:
    case SelectedRole:
        return false;
    case SourceRowRole:
        return sourceRow;
    default:
        return m_source->data(m_source->index(sourceRow), role);
    }
}

QHash<int, QByteArray> PlaylistView::roleNames() const {
    QHash<int, QByteArray> names = m_source ? m_source->roleNames() : PlaylistModel().roleNames();
    names.insert(IsHeaderRole, "isHeader");
    names.insert(SelectedRole, "selected");
    names.insert(SourceRowRole, "sourceRow");
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
    beginResetModel();
    m_source = source;
    rebuild();
    endResetModel();

    if (m_source) {
        connect(m_source, &QAbstractItemModel::rowsAboutToBeInserted, this,
                [this](const QModelIndex &, int first, int last) { beginInsertRows(QModelIndex(), first, last); });
        connect(m_source, &QAbstractItemModel::rowsInserted, this, [this] {
            rebuild();
            endInsertRows();
            emit visibleEntriesChanged();
            updateNavigation();
        });
        connect(m_source, &QAbstractItemModel::rowsAboutToBeRemoved, this,
                [this](const QModelIndex &, int first, int last) { beginRemoveRows(QModelIndex(), first, last); });
        connect(m_source, &QAbstractItemModel::rowsRemoved, this, [this] {
            rebuild();
            endRemoveRows();
            emit visibleEntriesChanged();
            updateNavigation();
        });
        connect(m_source, &QAbstractItemModel::modelAboutToBeReset, this, [this] { beginResetModel(); });
        connect(m_source, &QAbstractItemModel::modelReset, this, [this] {
            rebuild();
            endResetModel();
            emit visibleEntriesChanged();
            updateNavigation();
        });
        connect(m_source, &QAbstractItemModel::dataChanged, this,
                [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles) {
                    const int first = viewRowFor(topLeft.row());
                    const int last = viewRowFor(bottomRight.row());
                    if (first >= 0 && last >= 0) {
                        emit dataChanged(index(first), index(last), roles);
                    }
                    if (roles.isEmpty() || roles.contains(PlaylistModel::AvailableRole)) {
                        updateNavigation();
                    }
                });
        connect(m_source, &PlaylistModel::currentRowChanged, this, [this] {
            emit currentViewRowChanged();
            updateNavigation();
        });
    }
    emit sourceChanged();
    emit currentViewRowChanged();
    emit visibleEntriesChanged();
    updateNavigation();
}

void PlaylistView::rebuild() {
    m_sourceRows.clear();
    const int count = m_source ? m_source->count() : 0;
    m_sourceRows.reserve(count);
    for (int row = 0; row < count; ++row) {
        m_sourceRows.append(row);
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

int PlaylistView::viewRowFor(int sourceRow) const {
    if (sourceRow < 0) {
        return -1;
    }
    return int(m_sourceRows.indexOf(sourceRow));
}

int PlaylistView::sourceRowFor(int viewRow) const { return viewRow >= 0 && viewRow < m_sourceRows.size() ? m_sourceRows[viewRow] : -1; }

bool PlaylistView::available(int viewRow) const {
    const PlaylistModel::Entry *entry = m_source ? m_source->entry(sourceRowFor(viewRow)) : nullptr;
    return entry && entry->availability != PlaylistModel::Unavailable;
}

int PlaylistView::nextRow(bool repeat) const {
    const int count = int(m_sourceRows.size());
    if (!m_source || count == 0) {
        return -1;
    }
    int start = currentViewRow() + 1;
    if (currentViewRow() < 0) {
        start = 0;
        const int anchor = m_source->anchorRow();
        if (anchor >= 0) {
            start = count;
            for (int row = 0; row < count; ++row) {
                if (m_sourceRows[row] >= anchor) {
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
            return m_sourceRows[row];
        }
    }
    return -1;
}

int PlaylistView::previousRow() const {
    const int count = int(m_sourceRows.size());
    if (!m_source || count == 0) {
        return -1;
    }
    int start = currentViewRow() - 1;
    if (currentViewRow() < 0) {
        start = -1;
        const int anchor = m_source->anchorRow();
        for (int row = 0; anchor >= 0 && row < count; ++row) {
            if (m_sourceRows[row] < anchor) {
                start = row;
            }
        }
    }
    for (int row = start; row >= 0; --row) {
        if (available(row)) {
            return m_sourceRows[row];
        }
    }
    return -1;
}

QList<int> PlaylistView::visibleIds() const {
    QList<int> ids;
    if (!m_source) {
        return ids;
    }
    ids.reserve(m_sourceRows.size());
    for (const int sourceRow : m_sourceRows) {
        ids.append(m_source->idAt(sourceRow));
    }
    return ids;
}
