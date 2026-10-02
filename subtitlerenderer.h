#ifndef SUBTITLERENDERER_H
#define SUBTITLERENDERER_H

#include "probemessage.h"

#include <QImage>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QPoint>
#include <QSize>
#include <QtQml/qqmlregistration.h>

#include <memory>

struct ass_library;
struct ass_renderer;
struct ass_track;

struct SubtitleFrame {
    QImage image;
    QPoint offset;
    QSize frameSize;
    bool changed = true;
};

class SubtitleEngine {
public:
    SubtitleEngine();
    ~SubtitleEngine();

    SubtitleEngine(const SubtitleEngine &) = delete;
    SubtitleEngine &operator=(const SubtitleEngine &) = delete;

    void addFont(const QString &name, const QByteArray &data);
    void setTrack(const QByteArray &header);
    void clearTrack();
    void reset();
    bool hasTrack() const;
    void addEvents(const QList<ProbeEvent> &events);
    void setScale(double scale);
    SubtitleFrame render(qint64 timeMs, const QSize &frameSize);

private:
    void ensureRenderer();

    mutable QMutex m_mutex;
    ass_library *m_library = nullptr;
    ass_renderer *m_renderer = nullptr;
    ass_track *m_track = nullptr;
    QSize m_frameSize;
    double m_scale = 1.0;
    bool m_dirty = true;
    SubtitleFrame m_last;
};

class SubtitleRenderer : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by the media player")
    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY changed)

public:
    explicit SubtitleRenderer(QObject *parent = nullptr);

    std::shared_ptr<SubtitleEngine> engine() const;

    void addFont(const QString &name, const QByteArray &data);
    void setTrack(const QByteArray &header);
    void clearTrack();
    void reset();
    bool hasTrack() const;
    void addEvents(const QList<ProbeEvent> &events);
    void setScale(double scale);

signals:
    void changed();

private:
    std::shared_ptr<SubtitleEngine> m_engine;
};

#endif // SUBTITLERENDERER_H
