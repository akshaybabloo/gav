#include "playlistmodel.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <algorithm>

namespace {

bool isHttp(const QUrl &url) {
    const QString scheme = url.scheme().toLower();
    return scheme == QLatin1String("http") || scheme == QLatin1String("https");
}

QUrl toUrl(const QVariant &value) {
    if (value.metaType() == QMetaType::fromType<QUrl>()) {
        return value.toUrl();
    }
    const QString text = value.toString();
    if (text.isEmpty()) {
        return {};
    }
    const QUrl url(text);
    return url.scheme().isEmpty() ? QUrl::fromLocalFile(text) : url;
}

QString duplicateKey(const QUrl &location) {
    if (location.isLocalFile()) {
        const QString path = QDir::cleanPath(location.toLocalFile());
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
        return path.toCaseFolded();
#else
        return path;
#endif
    }
    return location.adjusted(QUrl::NormalizePathSegments).toString(QUrl::FullyEncoded);
}

QString fallbackTitle(const QUrl &location) {
    if (location.isLocalFile()) {
        return QFileInfo(location.toLocalFile()).fileName();
    }
    const QString name = location.fileName(QUrl::FullyDecoded);
    return name.isEmpty() ? location.toString() : name;
}

}

PlaylistModel::PlaylistModel(QObject *parent)
    : QAbstractListModel(parent),
      m_audioExtensions{QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("ogg"), QStringLiteral("flac"),
                        QStringLiteral("aac"), QStringLiteral("wma"), QStringLiteral("m4a")} {}

int PlaylistModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : int(m_entries.size()); }

QVariant PlaylistModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }
    const Entry &entry = m_entries[index.row()];
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return entry.title;
    case EntryIdRole:
        return entry.id;
    case LocationRole:
        return entry.location;
    case KindRole:
        return entry.kind;
    case StreamStateRole:
        return entry.streamState;
    case GroupRole:
        return entry.group;
    case LogoRole:
        return entry.logo;
    case DurationMsRole:
        return entry.durationMs;
    case AvailableRole:
        return entry.availability != Unavailable;
    case ReasonRole:
        return entry.reason;
    case IsCurrentRole:
        return entry.id == m_currentId;
    case QueuePositionRole:
        return int(m_queue.indexOf(entry.id)) + 1;
    default:
        return {};
    }
}

QHash<int, QByteArray> PlaylistModel::roleNames() const {
    return {{EntryIdRole, "entryId"},     {LocationRole, "location"},       {TitleRole, "title"},
            {KindRole, "kind"},           {StreamStateRole, "streamState"}, {GroupRole, "group"},
            {LogoRole, "logo"},           {DurationMsRole, "durationMs"},   {AvailableRole, "available"},
            {ReasonRole, "reason"},       {IsCurrentRole, "isCurrent"},     {QueuePositionRole, "queuePosition"}};
}

int PlaylistModel::count() const { return int(m_entries.size()); }

int PlaylistModel::currentRow() const { return rowForId(m_currentId); }

int PlaylistModel::currentId() const { return m_currentId; }

int PlaylistModel::anchorRow() const { return m_anchorRow; }

QStringList PlaylistModel::audioExtensions() const { return m_audioExtensions; }

int PlaylistModel::queueLength() const { return int(m_queue.size()); }

bool PlaylistModel::canUndoRemove() const { return !m_undoStep.isEmpty(); }

void PlaylistModel::setUndoStep(const QList<Removed> &step) {
    const bool could = canUndoRemove();
    m_undoStep = step;
    if (could != canUndoRemove()) {
        emit undoChanged();
    }
}

void PlaylistModel::setQueue(const QList<int> &queue) {
    if (queue == m_queue) {
        return;
    }
    QSet<int> affected(m_queue.cbegin(), m_queue.cend());
    affected.unite(QSet<int>(queue.cbegin(), queue.cend()));
    m_queue = queue;
    for (const int id : affected) {
        const int row = rowForId(id);
        if (row >= 0) {
            emit dataChanged(index(row), index(row), {QueuePositionRole});
        }
    }
    emit queueChanged();
}

void PlaylistModel::setAudioExtensions(const QStringList &extensions) {
    QStringList lowered;
    for (const QString &extension : extensions) {
        lowered.append(extension.toLower());
    }
    if (lowered == m_audioExtensions) {
        return;
    }
    m_audioExtensions = lowered;
    emit audioExtensionsChanged();
}

const PlaylistModel::Entry *PlaylistModel::entry(int row) const { return row >= 0 && row < m_entries.size() ? &m_entries[row] : nullptr; }

void PlaylistModel::setCurrentRow(int row) { setCurrentId(idAt(row)); }

void PlaylistModel::setCurrentId(int id) {
    if (id == m_currentId) {
        return;
    }
    const int previousRow = currentRow();
    m_currentId = id;
    const int row = currentRow();
    if (row >= 0) {
        m_anchorRow = -1;
    }
    for (const int changed : {previousRow, row}) {
        if (changed >= 0) {
            emit dataChanged(index(changed), index(changed), {IsCurrentRole});
        }
    }
    emit currentChanged();
    emit currentRowChanged();
}

bool PlaylistModel::makeEntry(const QVariantMap &map, Entry *entry) {
    const QUrl location = toUrl(map.contains(QStringLiteral("path")) ? map.value(QStringLiteral("path")) : map.value(QStringLiteral("location")));
    if (!location.isValid() || !(location.isLocalFile() || isHttp(location))) {
        return false;
    }
    entry->location = location;
    entry->title = map.value(QStringLiteral("title")).toString().trimmed();
    if (entry->title.isEmpty()) {
        entry->title = fallbackTitle(location);
    }
    if (isHttp(location)) {
        entry->kind = Stream;
    } else {
        const QString suffix = QFileInfo(location.toLocalFile()).suffix().toLower();
        entry->kind = m_audioExtensions.contains(suffix) ? LocalAudio : LocalVideo;
    }
    entry->group = map.value(QStringLiteral("group")).toString();
    const QUrl logo = toUrl(map.value(QStringLiteral("logo")));
    entry->logo = isHttp(logo) ? logo : QUrl();
    entry->attributes = map.value(QStringLiteral("attributes")).toString();
    const qint64 seconds = map.contains(QStringLiteral("durationSec")) ? map.value(QStringLiteral("durationSec")).toLongLong() : -1;
    entry->durationMs = seconds > 0 ? seconds * 1000 : -1;
    if (map.contains(QStringLiteral("available")) && !map.value(QStringLiteral("available")).toBool()) {
        entry->availability = Unavailable;
        entry->reason = map.value(QStringLiteral("reason")).toString();
    }
    return true;
}

int PlaylistModel::append(const QVariantList &entries) { return insert(count(), entries); }

int PlaylistModel::insert(int row, const QVariantList &entries) {
    const int at = qBound(0, row, count());
    QList<Entry> accepted;
    for (const QVariant &value : entries) {
        Entry entry;
        if (makeEntry(value.toMap(), &entry)) {
            entry.id = m_nextId++;
            accepted.append(entry);
        }
    }
    if (accepted.isEmpty()) {
        return -1;
    }

    setUndoStep({});
    const int previousCurrentRow = currentRow();
    beginInsertRows(QModelIndex(), at, at + int(accepted.size()) - 1);
    for (qsizetype i = 0; i < accepted.size(); ++i) {
        m_entries.insert(at + i, accepted[i]);
    }
    if (m_anchorRow >= at) {
        m_anchorRow += int(accepted.size());
    }
    endInsertRows();
    emit countChanged();
    if (currentRow() != previousCurrentRow) {
        emit currentRowChanged();
    }
    return at;
}

int PlaylistModel::remove(const QList<int> &ids) {
    QList<int> rows;
    for (const int id : ids) {
        const int row = rowForId(id);
        if (row >= 0 && !rows.contains(row)) {
            rows.append(row);
        }
    }
    if (rows.isEmpty()) {
        return 0;
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    const int previousCurrentRow = currentRow();
    const bool removesCurrent = previousCurrentRow >= 0 && rows.contains(previousCurrentRow);
    int anchor = removesCurrent ? previousCurrentRow : m_anchorRow;
    anchor -= int(std::count_if(rows.cbegin(), rows.cend(), [anchor](int row) { return row < anchor; }));

    QList<Removed> step;
    QList<int> queue = m_queue;
    for (auto row = rows.crbegin(); row != rows.crend(); ++row) {
        step.append({*row, m_entries[*row]});
        queue.removeAll(m_entries[*row].id);
    }

    for (qsizetype i = 0; i < rows.size();) {
        const int last = rows[i];
        int first = last;
        while (i + 1 < rows.size() && rows[i + 1] == first - 1) {
            first = rows[++i];
        }
        ++i;
        beginRemoveRows(QModelIndex(), first, last);
        m_entries.remove(first, last - first + 1);
        endRemoveRows();
    }
    m_anchorRow = m_entries.isEmpty() ? -1 : anchor;
    emit countChanged();
    setQueue(queue);
    setUndoStep(step);
    if (removesCurrent) {
        m_currentId = -1;
        emit currentChanged();
        emit currentRowChanged();
    } else if (currentRow() != previousCurrentRow) {
        emit currentRowChanged();
    }
    return int(rows.size());
}

int PlaylistModel::undoRemove() {
    if (m_undoStep.isEmpty()) {
        return 0;
    }
    const QList<Removed> step = m_undoStep;
    const int previousCurrentRow = currentRow();
    for (qsizetype i = 0; i < step.size();) {
        qsizetype last = i;
        while (last + 1 < step.size() && step[last + 1].row == step[last].row + 1) {
            ++last;
        }
        const int first = qMin(step[i].row, count());
        const int size = int(last - i + 1);
        beginInsertRows(QModelIndex(), first, first + size - 1);
        for (int offset = 0; offset < size; ++offset) {
            m_entries.insert(first + offset, step[i + offset].entry);
        }
        if (m_anchorRow >= first) {
            m_anchorRow += size;
        }
        endInsertRows();
        i = last + 1;
    }
    emit countChanged();
    setUndoStep({});
    if (currentRow() != previousCurrentRow) {
        emit currentRowChanged();
    }
    return int(step.size());
}

bool PlaylistModel::move(const QList<int> &ids, int destinationRow) {
    QList<int> rows;
    for (const int id : ids) {
        const int row = rowForId(id);
        if (row >= 0 && !rows.contains(row)) {
            rows.append(row);
        }
    }
    if (rows.isEmpty()) {
        return false;
    }
    std::sort(rows.begin(), rows.end());
    const int destination = qBound(0, destinationRow, count());
    const int above = int(std::count_if(rows.cbegin(), rows.cend(), [destination](int row) { return row < destination; }));

    const int previousCurrentRow = currentRow();
    bool changed = false;
    int target = destination;
    for (int i = above - 1; i >= 0; --i) {
        const int row = rows[i];
        if (row + 1 != target && beginMoveRows(QModelIndex(), row, row, QModelIndex(), target)) {
            m_entries.move(row, target - 1);
            endMoveRows();
            changed = true;
        }
        --target;
    }
    target = destination;
    for (int i = above; i < rows.size(); ++i) {
        const int row = rows[i];
        if (row != target && beginMoveRows(QModelIndex(), row, row, QModelIndex(), target)) {
            m_entries.move(row, target);
            endMoveRows();
            changed = true;
        }
        ++target;
    }
    if (!changed) {
        return false;
    }
    setUndoStep({});
    if (currentRow() != previousCurrentRow) {
        emit currentRowChanged();
    }
    return true;
}

int PlaylistModel::removeDuplicates() {
    QSet<QString> seen;
    QList<int> duplicates;
    for (const Entry &entry : m_entries) {
        const QString key = duplicateKey(entry.location);
        if (seen.contains(key)) {
            duplicates.append(entry.id);
        } else {
            seen.insert(key);
        }
    }
    return duplicates.isEmpty() ? 0 : remove(duplicates);
}

void PlaylistModel::playNext(int id) {
    if (rowForId(id) < 0 || m_queue.contains(id)) {
        return;
    }
    QList<int> queue = m_queue;
    queue.append(id);
    setQueue(queue);
}

int PlaylistModel::takeQueued() {
    QList<int> queue = m_queue;
    int taken = -1;
    while (!queue.isEmpty() && taken < 0) {
        const Entry *candidate = entry(rowForId(queue.takeFirst()));
        if (candidate && candidate->availability != Unavailable) {
            taken = rowForId(candidate->id);
        }
    }
    setQueue(queue);
    return taken;
}

void PlaylistModel::clear() {
    if (m_entries.isEmpty()) {
        return;
    }
    const bool hadCurrent = m_currentId >= 0;
    const bool hadQueue = !m_queue.isEmpty();
    beginResetModel();
    m_entries.clear();
    m_queue.clear();
    m_currentId = -1;
    m_anchorRow = -1;
    endResetModel();
    emit countChanged();
    setUndoStep({});
    if (hadQueue) {
        emit queueChanged();
    }
    if (hadCurrent) {
        emit currentChanged();
        emit currentRowChanged();
    }
}

void PlaylistModel::setLoaded(int id, qint64 durationMs, bool isLive) {
    const int row = rowForId(id);
    if (row < 0) {
        return;
    }
    Entry &entry = m_entries[row];
    QList<int> roles;
    if (entry.availability != Playable) {
        entry.availability = Playable;
        roles.append(AvailableRole);
    }
    if (!entry.reason.isEmpty()) {
        entry.reason.clear();
        roles.append(ReasonRole);
    }
    const StreamState state = entry.kind != Stream ? StreamUnknown : (isLive ? StreamLive : StreamOnDemand);
    if (entry.streamState != state) {
        entry.streamState = state;
        roles.append(StreamStateRole);
    }
    const qint64 duration = isLive ? -1 : (durationMs > 0 ? durationMs : entry.durationMs);
    if (entry.durationMs != duration) {
        entry.durationMs = duration;
        roles.append(DurationMsRole);
    }
    if (!roles.isEmpty()) {
        emit dataChanged(index(row), index(row), roles);
    }
}

void PlaylistModel::setUnavailable(int id, const QString &reason) {
    const int row = rowForId(id);
    if (row < 0) {
        return;
    }
    Entry &entry = m_entries[row];
    if (entry.availability == Unavailable && entry.reason == reason) {
        return;
    }
    entry.availability = Unavailable;
    entry.reason = reason;
    emit dataChanged(index(row), index(row), {AvailableRole, ReasonRole});
}

int PlaylistModel::rowForId(int id) const {
    if (id < 0) {
        return -1;
    }
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == id) {
            return int(i);
        }
    }
    return -1;
}

int PlaylistModel::idAt(int row) const { return row >= 0 && row < m_entries.size() ? m_entries[row].id : -1; }

QVariantMap PlaylistModel::toMap(const Entry &entry) const {
    return {{QStringLiteral("entryId"), entry.id},
            {QStringLiteral("path"), entry.location.toString()},
            {QStringLiteral("title"), entry.title},
            {QStringLiteral("kind"), entry.kind},
            {QStringLiteral("streamState"), entry.streamState},
            {QStringLiteral("group"), entry.group},
            {QStringLiteral("logo"), entry.logo.toString()},
            {QStringLiteral("attributes"), entry.attributes},
            {QStringLiteral("durationMs"), entry.durationMs},
            {QStringLiteral("durationSec"), entry.durationMs > 0 ? qRound64(entry.durationMs / 1000.0) : -1},
            {QStringLiteral("available"), entry.availability != Unavailable},
            {QStringLiteral("reason"), entry.reason}};
}

QVariantMap PlaylistModel::entryAt(int row) const {
    const Entry *found = entry(row);
    return found ? toMap(*found) : QVariantMap();
}

QVariantList PlaylistModel::toVariantList() const {
    QVariantList list;
    for (const Entry &entry : m_entries) {
        list.append(toMap(entry));
    }
    return list;
}

QList<QUrl> PlaylistModel::locations() const {
    QList<QUrl> list;
    for (const Entry &entry : m_entries) {
        list.append(entry.location);
    }
    return list;
}
