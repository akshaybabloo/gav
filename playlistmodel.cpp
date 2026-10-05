#include "playlistmodel.h"

#include <QFileInfo>

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
        return 0;
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
    if (removesCurrent) {
        m_currentId = -1;
        emit currentChanged();
        emit currentRowChanged();
    } else if (currentRow() != previousCurrentRow) {
        emit currentRowChanged();
    }
    return int(rows.size());
}

void PlaylistModel::clear() {
    if (m_entries.isEmpty()) {
        return;
    }
    const bool hadCurrent = m_currentId >= 0;
    beginResetModel();
    m_entries.clear();
    m_currentId = -1;
    m_anchorRow = -1;
    endResetModel();
    emit countChanged();
    if (hadCurrent) {
        emit currentChanged();
        emit currentRowChanged();
    }
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
