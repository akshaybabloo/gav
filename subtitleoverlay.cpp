#include "subtitleoverlay.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QVideoFrame>

#include <cmath>

SubtitleOverlay::SubtitleOverlay(QQuickItem *parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    m_pool.setMaxThreadCount(1);
}

SubtitleOverlay::~SubtitleOverlay() {
    QObject::disconnect(m_frameConnection);
    QObject::disconnect(m_rendererConnection);
    m_pool.waitForDone();
}

QVideoSink *SubtitleOverlay::videoSink() const { return m_videoSink; }

void SubtitleOverlay::setVideoSink(QVideoSink *sink) {
    if (m_videoSink == sink) {
        return;
    }
    QObject::disconnect(m_frameConnection);
    m_videoSink = sink;
    if (m_videoSink) {
        m_frameConnection = connect(m_videoSink, &QVideoSink::videoFrameChanged, this, &SubtitleOverlay::onVideoFrame);
    }
    emit videoSinkChanged();
}

SubtitleRenderer *SubtitleOverlay::renderer() const { return m_renderer; }

void SubtitleOverlay::setRenderer(SubtitleRenderer *renderer) {
    if (m_renderer == renderer) {
        return;
    }
    QObject::disconnect(m_rendererConnection);
    m_renderer = renderer;
    if (m_renderer) {
        m_rendererConnection = connect(m_renderer, &SubtitleRenderer::changed, this, &SubtitleOverlay::requestRender);
    }
    emit rendererChanged();
    requestRender();
}

int SubtitleOverlay::delayMs() const { return m_delayMs; }

void SubtitleOverlay::setDelayMs(int delayMs) {
    if (m_delayMs == delayMs) {
        return;
    }
    m_delayMs = delayMs;
    emit delayMsChanged();
    requestRender();
}

QRectF SubtitleOverlay::contentRect() const { return m_contentRect; }

void SubtitleOverlay::setContentRect(const QRectF &rect) {
    if (m_contentRect == rect) {
        return;
    }
    m_contentRect = rect;
    emit contentRectChanged();
    requestRender();
}

void SubtitleOverlay::itemChange(ItemChange change, const ItemChangeData &value) {
    QQuickItem::itemChange(change, value);
    if (change == ItemDevicePixelRatioHasChanged || change == ItemSceneChange || change == ItemVisibleHasChanged) {
        requestRender();
    }
}

void SubtitleOverlay::onVideoFrame(const QVideoFrame &frame) {
    if (!frame.isValid() || frame.startTime() < 0) {
        return;
    }
    const qint64 frameMs = frame.startTime() / 1000;
    if (frameMs == m_lastFrameMs) {
        return;
    }
    m_lastFrameMs = frameMs;
    requestRender();
}

QSize SubtitleOverlay::renderSize() const {
    const qreal ratio = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    return QSize(int(std::lround(m_contentRect.width() * ratio)), int(std::lround(m_contentRect.height() * ratio)));
}

void SubtitleOverlay::requestRender() {
    if (m_busy) {
        m_pending = true;
        return;
    }

    const QSize size = renderSize();
    if (!m_renderer || !m_renderer->hasTrack() || !isVisible() || m_lastFrameMs < 0 || size.isEmpty()) {
        if (!m_image.isNull()) {
            m_image = QImage();
            m_imageDirty = true;
            update();
        }
        return;
    }

    m_busy = true;
    m_pending = false;
    const std::shared_ptr<SubtitleEngine> engine = m_renderer->engine();
    const qint64 timeMs = m_lastFrameMs - m_delayMs;
    m_pool.start([this, engine, timeMs, size] {
        const SubtitleFrame frame = engine->render(timeMs, size);
        QMetaObject::invokeMethod(this, [this, frame] { onRendered(frame); }, Qt::QueuedConnection);
    });
}

void SubtitleOverlay::onRendered(const SubtitleFrame &frame) {
    m_busy = false;
    if (frame.changed) {
        m_image = frame.image;
        m_offset = frame.offset;
        m_frameSize = frame.frameSize;
        m_imageDirty = true;
        update();
    }
    if (m_pending) {
        requestRender();
    }
}

QSGNode *SubtitleOverlay::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (m_image.isNull() || m_frameSize.isEmpty() || !window()) {
        delete node;
        m_imageDirty = false;
        return nullptr;
    }

    if (!node) {
        node = new QSGSimpleTextureNode;
        node->setOwnsTexture(true);
        node->setFiltering(QSGTexture::Linear);
        m_imageDirty = true;
    }
    if (m_imageDirty) {
        node->setTexture(window()->createTextureFromImage(m_image, QQuickWindow::TextureHasAlphaChannel));
        m_imageDirty = false;
    }

    const qreal scaleX = m_contentRect.width() / m_frameSize.width();
    const qreal scaleY = m_contentRect.height() / m_frameSize.height();
    node->setRect(QRectF(m_contentRect.x() + m_offset.x() * scaleX, m_contentRect.y() + m_offset.y() * scaleY, m_image.width() * scaleX,
                         m_image.height() * scaleY));
    return node;
}
