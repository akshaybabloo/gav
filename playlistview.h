#ifndef PLAYLISTVIEW_H
#define PLAYLISTVIEW_H

#include "playlistmodel.h"

#include <QAbstractListModel>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QtQml/qqmlregistration.h>

class PlaylistView : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(PlaylistModel *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(Filter filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(SortOrder sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortOrderChanged)
    Q_PROPERTY(bool grouped READ grouped WRITE setGrouped NOTIFY groupedChanged)
    Q_PROPERTY(int matchCount READ matchCount NOTIFY matchCountChanged)
    Q_PROPERTY(bool hasGroups READ hasGroups NOTIFY hasGroupsChanged)
    Q_PROPERTY(bool canReorder READ canReorder NOTIFY canReorderChanged)
    Q_PROPERTY(int currentViewRow READ currentViewRow NOTIFY currentViewRowChanged)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY navigationChanged)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY navigationChanged)

public:
    enum Filter { All, LocalFiles, Streams };
    Q_ENUM(Filter)

    enum SortOrder { PlaylistOrder, Title, Duration };
    Q_ENUM(SortOrder)

    enum Role {
        IsHeaderRole = Qt::UserRole + 100,
        SelectedRole,
        SourceRowRole,
        GroupCountRole,
        CollapsedRole,
    };

    explicit PlaylistView(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    PlaylistModel *source() const;
    void setSource(PlaylistModel *source);
    QString searchText() const;
    void setSearchText(const QString &text);
    Filter filter() const;
    void setFilter(Filter filter);
    SortOrder sortOrder() const;
    void setSortOrder(SortOrder order);
    bool grouped() const;
    void setGrouped(bool grouped);
    int matchCount() const;
    bool hasGroups() const;
    bool canReorder() const;
    int currentViewRow() const;
    bool canGoNext() const;
    bool canGoPrevious() const;

    Q_INVOKABLE void toggleGroup(const QString &group);
    Q_INVOKABLE void clearSearchAndFilter();
    Q_INVOKABLE int viewRowFor(int sourceRow) const;
    Q_INVOKABLE int sourceRowFor(int viewRow) const;
    Q_INVOKABLE int nextRow(bool repeat) const;
    Q_INVOKABLE int previousRow() const;
    Q_INVOKABLE QList<int> visibleIds() const;
    Q_INVOKABLE QList<int> playableIds() const;

signals:
    void sourceChanged();
    void searchTextChanged();
    void filterChanged();
    void sortOrderChanged();
    void groupedChanged();
    void matchCountChanged();
    void hasGroupsChanged();
    void canReorderChanged();
    void currentViewRowChanged();
    void navigationChanged();
    void visibleEntriesChanged();
    void playableEntriesChanged();
    void addedEntriesHidden(int added, int hidden);

private:
    struct Row {
        int sourceRow = -1;
        int group = -1;
    };

    struct Group {
        QString name;
        int count = 0;
        bool collapsed = false;
    };

    struct Folded {
        QString title;
        QString group;
    };

    struct Layout {
        QList<Row> rows;
        QList<Group> groups;
        int matchCount = 0;
    };

    struct State {
        bool hasGroups = false;
        bool canReorder = true;
        int matchCount = 0;
    };

    bool groupingActive() const;
    bool matches(int sourceRow) const;
    Folded fold(int sourceRow) const;
    void refold();
    Layout build() const;
    void assign(const Layout &layout);
    void resetLayout(const State &before);
    void updateCurrent();
    void removeHiddenRows();
    State state() const;
    void notify(const State &before);
    void updateNavigation();
    bool available(int viewRow) const;

    QPointer<PlaylistModel> m_source;
    QList<Row> m_rows;
    QList<Group> m_groups;
    QList<int> m_viewRows;
    QList<Folded> m_folded;
    QSet<QString> m_collapsed;
    QString m_searchText;
    QString m_needle;
    Filter m_filter = All;
    SortOrder m_sortOrder = PlaylistOrder;
    bool m_grouped = false;
    int m_matchCount = 0;
    int m_entriesWithGroup = 0;
    int m_currentViewRow = -1;
    State m_before;
    bool m_forwarding = false;
    bool m_canGoNext = false;
    bool m_canGoPrevious = false;
};

#endif // PLAYLISTVIEW_H
