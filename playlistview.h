#ifndef PLAYLISTVIEW_H
#define PLAYLISTVIEW_H

#include "playlistmodel.h"

#include <QAbstractListModel>
#include <QList>
#include <QPointer>
#include <QtQml/qqmlregistration.h>

class PlaylistView : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(PlaylistModel *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int currentViewRow READ currentViewRow NOTIFY currentViewRowChanged)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY navigationChanged)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY navigationChanged)

public:
    enum Role {
        IsHeaderRole = Qt::UserRole + 100,
        SelectedRole,
        SourceRowRole,
    };

    explicit PlaylistView(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    PlaylistModel *source() const;
    void setSource(PlaylistModel *source);
    int currentViewRow() const;
    bool canGoNext() const;
    bool canGoPrevious() const;

    Q_INVOKABLE int viewRowFor(int sourceRow) const;
    Q_INVOKABLE int sourceRowFor(int viewRow) const;
    Q_INVOKABLE int nextRow(bool repeat) const;
    Q_INVOKABLE int previousRow() const;
    Q_INVOKABLE QList<int> visibleIds() const;
    Q_INVOKABLE QList<int> playableIds() const;

signals:
    void sourceChanged();
    void currentViewRowChanged();
    void navigationChanged();
    void visibleEntriesChanged();
    void playableEntriesChanged();

private:
    void rebuild();
    void updateNavigation();
    bool available(int viewRow) const;

    QPointer<PlaylistModel> m_source;
    QList<int> m_sourceRows;
    bool m_canGoNext = false;
    bool m_canGoPrevious = false;
};

#endif // PLAYLISTVIEW_H
