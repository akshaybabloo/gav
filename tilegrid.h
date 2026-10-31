#ifndef TILEGRID_H
#define TILEGRID_H

#include <QList>
#include <QObject>
#include <QRectF>
#include <QSet>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class TileGrid : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(bool splitView READ splitView NOTIFY changed)
    Q_PROPERTY(int selectedSlot READ selectedSlot NOTIFY changed)
    Q_PROPERTY(bool focused READ focused NOTIFY changed)
    Q_PROPERTY(int focusedSlot READ focusedSlot NOTIFY changed)
    Q_PROPERTY(QList<int> entryIds READ entryIds NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    static constexpr int maxTiles = 4;

    explicit TileGrid(QObject *parent = nullptr);

    int count() const;
    bool splitView() const;
    int selectedSlot() const;
    bool focused() const;
    int focusedSlot() const;
    QList<int> entryIds() const;
    int revision() const;

    Q_INVOKABLE int positionOf(int slot) const;
    Q_INVOKABLE int slotAtPosition(int position) const;
    Q_INVOKABLE int slotOf(int entryId) const;
    Q_INVOKABLE QRectF cellFor(int slot, qreal width, qreal height) const;
    Q_INVOKABLE int slotAt(qreal x, qreal y, qreal width, qreal height) const;
    Q_INVOKABLE int resultingCount(const QList<int> &entryIds) const;
    Q_INVOKABLE bool canAdd(const QList<int> &entryIds) const;

    Q_INVOKABLE QVariantMap open(int entryId);
    Q_INVOKABLE QVariantMap start(const QList<int> &entryIds);
    Q_INVOKABLE QVariantMap add(const QList<int> &entryIds);
    Q_INVOKABLE QVariantMap replace(int slot, int entryId);
    Q_INVOKABLE QVariantMap close(int slot);
    Q_INVOKABLE QVariantMap end();
    Q_INVOKABLE QVariantMap entryRemoved(int entryId);
    Q_INVOKABLE void clear();

    Q_INVOKABLE void select(int slot);
    Q_INVOKABLE QVariantMap pressPosition(int position, const QList<int> &playingFiles);
    Q_INVOKABLE QVariantMap doubleClick(int slot, bool fullScreen, const QList<int> &playingFiles);
    Q_INVOKABLE QVariantMap escape(bool fullScreen);

signals:
    void changed();

private:
    void notifyChanged();
    QList<int> newEntries(const QList<int> &entryIds) const;
    int freeSlot() const;
    QList<int> fill(const QList<int> &entryIds);
    QList<int> leaveFocus();
    QRectF cellAtPosition(int position, qreal width, qreal height) const;

    QList<int> m_entries;
    QList<int> m_order;
    int m_selected = 0;
    int m_focused = -1;
    QSet<int> m_pausedByFocus;
    int m_revision = 0;
};

#endif // TILEGRID_H
