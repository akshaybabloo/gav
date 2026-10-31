#ifndef RTSPOPENGATE_H
#define RTSPOPENGATE_H

#include <QList>
#include <QObject>

class RtspOpenGate : public QObject {
    Q_OBJECT

public:
    explicit RtspOpenGate(QObject *parent = nullptr);

    static RtspOpenGate &instance();

    bool request(int id);
    void release(int id);
    void cancel(int id);
    void beginOtherOpen();
    void endOtherOpen();

    bool holds(int id) const;
    bool waiting(int id) const;
    bool overlapped(int id) const;

signals:
    void admitted(int id);

private:
    void admitNext();

    int m_holder = -1;
    QList<int> m_waiting;
    int m_otherOpens = 0;
    bool m_overlapped = false;
};

#endif // RTSPOPENGATE_H
