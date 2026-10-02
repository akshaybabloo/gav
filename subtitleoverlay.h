#ifndef SUBTITLEOVERLAY_H
#define SUBTITLEOVERLAY_H

#include "subtitlerenderer.h"

#include <QImage>
#include <QPointer>
#include <QQuickItem>
#include <QRectF>
#include <QThreadPool>
#include <QVideoSink>
#include <QtQml/qqmlregistration.h>

class SubtitleOverlay : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVideoSink *videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)
    Q_PROPERTY(SubtitleRenderer *renderer READ renderer WRITE setRenderer NOTIFY rendererChanged)
    Q_PROPERTY(int delayMs READ delayMs WRITE setDelayMs NOTIFY delayMsChanged)
    Q_PROPERTY(QRectF contentRect READ contentRect WRITE setContentRect NOTIFY contentRectChanged)

public:
    explicit SubtitleOverlay(QQuickItem *parent = nullptr);
    ~SubtitleOverlay() override;

    QVideoSink *videoSink() const;
    void setVideoSink(QVideoSink *sink);
    SubtitleRenderer *renderer() const;
    void setRenderer(SubtitleRenderer *renderer);
    int delayMs() const;
    void setDelayMs(int delayMs);
    QRectF contentRect() const;
    void setContentRect(const QRectF &rect);

signals:
    void videoSinkChanged();
    void rendererChanged();
    void delayMsChanged();
    void contentRectChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    void onVideoFrame(const QVideoFrame &frame);
    void requestRender();
    void onRendered(const SubtitleFrame &frame);
    QSize renderSize() const;

    QPointer<QVideoSink> m_videoSink;
    QPointer<SubtitleRenderer> m_renderer;
    QMetaObject::Connection m_frameConnection;
    QMetaObject::Connection m_rendererConnection;
    QThreadPool m_pool;
    int m_delayMs = 0;
    QRectF m_contentRect;
    qint64 m_lastFrameMs = -1;
    bool m_busy = false;
    bool m_pending = false;
    QImage m_image;
    QPoint m_offset;
    QSize m_frameSize;
    bool m_imageDirty = false;
};

#endif // SUBTITLEOVERLAY_H
