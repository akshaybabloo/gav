#ifndef SHUFFLEORDER_H
#define SHUFFLEORDER_H

#include <QList>
#include <QObject>
#include <QRandomGenerator>
#include <QtQml/qqmlregistration.h>

class ShuffleOrder : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int remaining READ remaining NOTIFY orderChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY orderChanged)

public:
    explicit ShuffleOrder(QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool enabled);
    int remaining() const;
    bool canGoBack() const;
    int current() const;
    void setSeed(quint32 seed);

    Q_INVOKABLE void reset(int count, int currentIndex);
    Q_INVOKABLE int next(bool repeatPlaylist);
    Q_INVOKABLE int previous();
    Q_INVOKABLE void setCurrent(int index);
    Q_INVOKABLE void itemInserted(int index);
    Q_INVOKABLE void itemRemoved(int index);
    Q_INVOKABLE void itemMoved(int from, int to);

signals:
    void enabledChanged();
    void orderChanged();

private:
    void refill(int exclude);
    static int remap(int value, int from, int to);

    bool m_enabled = false;
    int m_count = 0;
    QList<int> m_remaining;
    QList<int> m_history;
    QRandomGenerator m_random;
};

#endif // SHUFFLEORDER_H
