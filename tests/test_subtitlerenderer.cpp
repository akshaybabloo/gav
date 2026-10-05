#include <gtest/gtest.h>
#include "../probeworker.h"
#include "../subtitlerenderer.h"

#include <QElapsedTimer>
#include <QRect>

#include <iostream>

namespace {

const QSize frameSize(1920, 1080);

int coveredPixels(const SubtitleFrame &frame, const QRect &area) {
    if (frame.image.isNull()) {
        return 0;
    }
    const QRect overlap = area.intersected(QRect(frame.offset, frame.image.size()));
    int count = 0;
    for (int y = overlap.top(); y <= overlap.bottom(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(frame.image.constScanLine(y - frame.offset.y()));
        for (int x = overlap.left(); x <= overlap.right(); ++x) {
            if (qAlpha(line[x - frame.offset.x()]) > 0) {
                ++count;
            }
        }
    }
    return count;
}

class SubtitleRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        QByteArray header;
        QList<ProbeEvent> events;
        ProbeWorker worker([&](const ProbeMessage &message) {
            if (message.type == ProbeMessage::Type::Header) {
                header = message.assHeader.toUtf8();
            } else if (message.type == ProbeMessage::Type::Event) {
                events.append(message.event);
            }
        });
        ProbeOptions options;
        options.subtitleFiles = {QStringLiteral(GAV_TEST_DATA_DIR "/styled.ass")};
        ASSERT_EQ(worker.run(options), 0);
        ASSERT_EQ(events.size(), 3);
        engine.setTrack(header);
        engine.addEvents(events);
    }

    SubtitleEngine engine;
};

}

TEST_F(SubtitleRendererTest, PositionedSignRendersTopLeft) {
    const SubtitleFrame frame = engine.render(5000, frameSize);
    EXPECT_GT(coveredPixels(frame, QRect(150, 150, 800, 150)), 100);
    EXPECT_EQ(coveredPixels(frame, QRect(0, 700, 1920, 380)), 0);
}

TEST_F(SubtitleRendererTest, RotatedSignRendersAroundItsAnchor) {
    const SubtitleFrame frame = engine.render(12000, frameSize);
    EXPECT_GT(coveredPixels(frame, QRect(1100, 300, 820, 700)), 100);
    EXPECT_EQ(coveredPixels(frame, QRect(0, 0, 900, 400)), 0);
}

TEST_F(SubtitleRendererTest, KaraokeLineRendersAtBottom) {
    const SubtitleFrame frame = engine.render(20000, frameSize);
    EXPECT_GT(coveredPixels(frame, QRect(500, 850, 920, 230)), 100);
    EXPECT_EQ(coveredPixels(frame, QRect(0, 0, 1920, 600)), 0);
}

TEST_F(SubtitleRendererTest, NothingBetweenEvents) {
    const SubtitleFrame frame = engine.render(9000, frameSize);
    EXPECT_TRUE(frame.image.isNull());
}

TEST_F(SubtitleRendererTest, UnchangedFrameIsReportedAsUnchanged) {
    engine.render(5000, frameSize);
    const SubtitleFrame again = engine.render(5000, frameSize);
    EXPECT_FALSE(again.changed);
    EXPECT_FALSE(again.image.isNull());
}

TEST_F(SubtitleRendererTest, ScaleChangesOutputSize) {
    const QSize normal = engine.render(20000, frameSize).image.size();
    engine.setScale(2.0);
    const QSize larger = engine.render(20000, frameSize).image.size();
    EXPECT_GT(larger.width(), normal.width());
}

TEST_F(SubtitleRendererTest, ClearedTrackRendersNothing) {
    engine.render(5000, frameSize);
    engine.clearTrack();
    const SubtitleFrame frame = engine.render(5000, frameSize);
    EXPECT_TRUE(frame.image.isNull());
    EXPECT_TRUE(frame.changed);
}

TEST_F(SubtitleRendererTest, ResetDropsTrackAndCanBeReused) {
    engine.addFont("unused.ttf", QByteArray(16, 'x'));
    EXPECT_FALSE(engine.render(5000, frameSize).image.isNull());
    engine.reset();
    EXPECT_FALSE(engine.hasTrack());
    EXPECT_TRUE(engine.render(5000, frameSize).image.isNull());

    SetUp();
    EXPECT_GT(coveredPixels(engine.render(5000, frameSize), QRect(150, 150, 800, 150)), 100);
}

TEST_F(SubtitleRendererTest, SettingTheSameTrackAgainStillRenders) {
    SetUp();
    SetUp();
    EXPECT_GT(coveredPixels(engine.render(5000, frameSize), QRect(150, 150, 800, 150)), 100);
}

TEST_F(SubtitleRendererTest, RenderTimeIsLogged) {
    engine.render(5000, frameSize);
    constexpr int frames = 120;
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < frames; ++i) {
        engine.render(i % 2 ? 12000 + i * 40 : 20000 + i * 40, frameSize);
    }
    const double averageMs = static_cast<double>(timer.nsecsElapsed()) / 1e6 / frames;
    std::cout << "Average subtitle render at 1080p: " << averageMs << " ms" << std::endl;
    EXPECT_GT(averageMs, 0.0);

    QList<ProbeEvent> animated;
    for (int line = 0; line < 3; ++line) {
        const QByteArray text = "{\\move(960," + QByteArray::number(700 + line * 110) + ",1000," + QByteArray::number(740 + line * 110) +
                                ")\\blur3\\bord4\\fad(4000,4000)}The quick brown fox jumps over the lazy dog, line " + QByteArray::number(line + 1);
        animated.append({30000, 10000, QByteArray::number(10 + line) + ",0,Default,,0,0,0,," + text});
    }
    engine.addEvents(animated);
    engine.render(30000, frameSize);
    timer.restart();
    int changedFrames = 0;
    double worstMs = 0;
    for (int i = 0; i < frames; ++i) {
        QElapsedTimer single;
        single.start();
        changedFrames += engine.render(30040 + i * 40, frameSize).changed ? 1 : 0;
        worstMs = qMax(worstMs, static_cast<double>(single.nsecsElapsed()) / 1e6);
    }
    const double animatedMs = static_cast<double>(timer.nsecsElapsed()) / 1e6 / frames;
    std::cout << "Three animated, blurred lines at 1080p: " << animatedMs << " ms average, " << worstMs << " ms worst, " << changedFrames << " of " << frames
              << " frames redrawn" << std::endl;
    EXPECT_EQ(changedFrames, frames);
}
