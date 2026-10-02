#include "subtitlerenderer.h"

#include <QMutexLocker>
#include <QRect>

#include <spdlog/spdlog.h>

extern "C" {
#include <ass/ass.h>
}

#include <cstdarg>
#include <cstdio>

namespace {

const QByteArray defaultHeader = QByteArrayLiteral(
    "[Script Info]\n"
    "ScriptType: v4.00+\n"
    "PlayResX: 384\n"
    "PlayResY: 288\n"
    "ScaledBorderAndShadow: yes\n\n"
    "[V4+ Styles]\n"
    "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
    "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
    "Style: Default,Arial,16,&Hffffff,&Hffffff,&H0,&H0,0,0,0,0,100,100,0,0,1,1,0,2,10,10,10,0\n\n"
    "[Events]\n"
    "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n");

void logLibassMessage(int level, const char *format, va_list arguments, void *) {
    if (level > 4) {
        return;
    }
    char buffer[512];
    std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    spdlog::debug("libass: {}", buffer);
}

void blend(QImage &target, const QPoint &origin, const ASS_Image *image) {
    const quint32 color = image->color;
    const int red = int(color >> 24);
    const int green = int((color >> 16) & 0xFF);
    const int blue = int((color >> 8) & 0xFF);
    const int opacity = 255 - int(color & 0xFF);
    if (opacity == 0) {
        return;
    }

    const int left = image->dst_x - origin.x();
    const int top = image->dst_y - origin.y();
    for (int y = 0; y < image->h; ++y) {
        const unsigned char *source = image->bitmap + y * image->stride;
        auto *destination = reinterpret_cast<QRgb *>(target.scanLine(top + y)) + left;
        for (int x = 0; x < image->w; ++x) {
            const int coverage = (source[x] * opacity + 127) / 255;
            if (coverage == 0) {
                continue;
            }
            const QRgb below = destination[x];
            const int remaining = 255 - coverage;
            destination[x] = qRgba((red * coverage + qRed(below) * remaining + 127) / 255, (green * coverage + qGreen(below) * remaining + 127) / 255,
                                   (blue * coverage + qBlue(below) * remaining + 127) / 255, coverage + (qAlpha(below) * remaining + 127) / 255);
        }
    }
}

}

SubtitleEngine::SubtitleEngine() {
    m_library = ass_library_init();
    if (m_library) {
        ass_set_message_cb(m_library, logLibassMessage, nullptr);
        ass_set_extract_fonts(m_library, 1);
    }
}

SubtitleEngine::~SubtitleEngine() {
    QMutexLocker locker(&m_mutex);
    if (m_track) {
        ass_free_track(m_track);
    }
    if (m_renderer) {
        ass_renderer_done(m_renderer);
    }
    if (m_library) {
        ass_library_done(m_library);
    }
}

void SubtitleEngine::addFont(const QString &name, const QByteArray &data) {
    QMutexLocker locker(&m_mutex);
    if (!m_library || data.isEmpty()) {
        return;
    }
    QByteArray fontName = name.toUtf8();
    ass_add_font(m_library, fontName.constData(), data.constData(), int(data.size()));
    m_dirty = true;
}

void SubtitleEngine::setTrack(const QByteArray &header) {
    QMutexLocker locker(&m_mutex);
    if (!m_library) {
        return;
    }
    if (m_track) {
        ass_free_track(m_track);
    }
    m_track = ass_new_track(m_library);
    if (m_track) {
        const QByteArray &codecPrivate = header.trimmed().isEmpty() ? defaultHeader : header;
        QByteArray copy = codecPrivate;
        ass_process_codec_private(m_track, copy.data(), int(copy.size()));
    }
    m_dirty = true;
}

void SubtitleEngine::clearTrack() {
    QMutexLocker locker(&m_mutex);
    if (m_track) {
        ass_free_track(m_track);
        m_track = nullptr;
    }
    m_dirty = true;
}

bool SubtitleEngine::hasTrack() const {
    QMutexLocker locker(&m_mutex);
    return m_track != nullptr;
}

void SubtitleEngine::addEvents(const QList<ProbeEvent> &events) {
    QMutexLocker locker(&m_mutex);
    if (!m_track) {
        return;
    }
    for (const ProbeEvent &event : events) {
        QByteArray data = event.ass;
        ass_process_chunk(m_track, data.data(), int(data.size()), event.startMs, event.durationMs);
    }
    m_dirty = true;
}

void SubtitleEngine::setScale(double scale) {
    QMutexLocker locker(&m_mutex);
    if (qFuzzyCompare(m_scale, scale)) {
        return;
    }
    m_scale = scale;
    if (m_renderer) {
        ass_set_font_scale(m_renderer, m_scale);
    }
    m_dirty = true;
}

void SubtitleEngine::ensureRenderer() {
    if (m_renderer || !m_library) {
        return;
    }
    m_renderer = ass_renderer_init(m_library);
    if (!m_renderer) {
        return;
    }
    ass_set_fonts(m_renderer, nullptr, "sans-serif", ASS_FONTPROVIDER_AUTODETECT, nullptr, 1);
    ass_set_font_scale(m_renderer, m_scale);
    m_frameSize = QSize();
}

SubtitleFrame SubtitleEngine::render(qint64 timeMs, const QSize &frameSize) {
    QMutexLocker locker(&m_mutex);
    if (!m_track || frameSize.isEmpty()) {
        const bool changed = !m_last.image.isNull();
        m_last = SubtitleFrame{QImage(), QPoint(), frameSize, changed};
        return m_last;
    }

    ensureRenderer();
    if (!m_renderer) {
        return SubtitleFrame{QImage(), QPoint(), frameSize, false};
    }
    if (frameSize != m_frameSize) {
        m_frameSize = frameSize;
        ass_set_frame_size(m_renderer, frameSize.width(), frameSize.height());
        m_dirty = true;
    }

    int detectChange = 0;
    const ASS_Image *images = ass_render_frame(m_renderer, m_track, timeMs, &detectChange);
    if (detectChange == 0 && !m_dirty) {
        SubtitleFrame unchanged = m_last;
        unchanged.changed = false;
        return unchanged;
    }
    m_dirty = false;

    QRect bounds;
    for (const ASS_Image *image = images; image; image = image->next) {
        if (image->w > 0 && image->h > 0) {
            bounds = bounds.united(QRect(image->dst_x, image->dst_y, image->w, image->h));
        }
    }
    bounds = bounds.intersected(QRect(QPoint(0, 0), frameSize));

    SubtitleFrame frame;
    frame.frameSize = frameSize;
    frame.changed = true;
    if (!bounds.isEmpty()) {
        frame.image = QImage(bounds.size(), QImage::Format_ARGB32_Premultiplied);
        frame.image.fill(Qt::transparent);
        frame.offset = bounds.topLeft();
        for (const ASS_Image *image = images; image; image = image->next) {
            if (image->w > 0 && image->h > 0) {
                blend(frame.image, frame.offset, image);
            }
        }
    }
    m_last = frame;
    return frame;
}

SubtitleRenderer::SubtitleRenderer(QObject *parent) : QObject(parent), m_engine(std::make_shared<SubtitleEngine>()) {}

std::shared_ptr<SubtitleEngine> SubtitleRenderer::engine() const { return m_engine; }

void SubtitleRenderer::addFont(const QString &name, const QByteArray &data) {
    m_engine->addFont(name, data);
    emit changed();
}

void SubtitleRenderer::setTrack(const QByteArray &header) {
    m_engine->setTrack(header);
    emit changed();
}

void SubtitleRenderer::clearTrack() {
    m_engine->clearTrack();
    emit changed();
}

bool SubtitleRenderer::hasTrack() const { return m_engine->hasTrack(); }

void SubtitleRenderer::addEvents(const QList<ProbeEvent> &events) {
    if (events.isEmpty()) {
        return;
    }
    m_engine->addEvents(events);
    emit changed();
}

void SubtitleRenderer::setScale(double scale) {
    m_engine->setScale(scale);
    emit changed();
}
