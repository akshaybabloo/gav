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

    Q_INVOKABLE void reset(const QList<int> &ids, int currentId);
    Q_INVOKABLE void setCandidates(const QList<int> &ids);
    Q_INVOKABLE int next(bool repeatPlaylist);
    Q_INVOKABLE int previous();
    Q_INVOKABLE void setCurrent(int id);

signals:
    void enabledChanged();
    void orderChanged();

private:
    void refill(int exclude);

    bool m_enabled = false;
    QList<int> m_candidates;
    QList<int> m_remaining;
    QList<int> m_history;
    QRandomGenerator m_random;
};

#endif // SHUFFLEORDER_H
