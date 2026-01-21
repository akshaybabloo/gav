#include <gtest/gtest.h>
#include "../collage.h"
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>
#include <QEventLoop>
#include <QTimer>

class CollageTest : public ::testing::Test {
protected:
    void SetUp() override {
        collage = new Collage();
        tempDir = new QTemporaryDir();
    }

    void TearDown() override {
        delete collage;
        delete tempDir;
    }

    // Helper to wait for async operations with timeout
    bool waitForSignal(QSignalSpy &spy, int timeoutMs = 5000) {
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);

        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(collage, &Collage::collageFinished, &loop, &QEventLoop::quit);

        timer.start(timeoutMs);
        loop.exec();

        return spy.count() > 0;
    }

    Collage *collage;
    QTemporaryDir *tempDir;
};

TEST_F(CollageTest, ObjectCreation) {
    EXPECT_NE(collage, nullptr);
}

TEST_F(CollageTest, SignalsExist) {
    // Test that the collage object has the expected signals
    QSignalSpy startedSpy(collage, &Collage::collageStarted);
    QSignalSpy progressSpy(collage, &Collage::collageProgress);
    QSignalSpy completedSpy(collage, &Collage::collageCompleted);
    QSignalSpy finishedSpy(collage, &Collage::collageFinished);

    EXPECT_TRUE(startedSpy.isValid());
    EXPECT_TRUE(progressSpy.isValid());
    EXPECT_TRUE(completedSpy.isValid());
    EXPECT_TRUE(finishedSpy.isValid());
}

TEST_F(CollageTest, EmptyListDoesNotCrash) {
    QList<QUrl> emptyList;

    QSignalSpy finishedSpy(collage, &Collage::collageFinished);

    // Should not crash with empty list
    collage->toCollage(emptyList);

    // Should emit finished signal with 0 success and 0 fail
    ASSERT_EQ(finishedSpy.count(), 1);
    QList<QVariant> arguments = finishedSpy.takeFirst();
    EXPECT_EQ(arguments.at(0).toInt(), 0); // successCount
    EXPECT_EQ(arguments.at(1).toInt(), 0); // failCount
}

TEST_F(CollageTest, InvalidPathHandling) {
    // Test createCollageSingle directly since toCollage spawns subprocesses
    // which won't work in the test executable (no --collage CLI handling)
    QUrl invalidPath("file:///nonexistent/video.mp4");

    // createCollageSingle should return empty string for invalid path
    QString result = Collage::createCollageSingle(invalidPath);

    EXPECT_TRUE(result.isEmpty()) << "Invalid path should return empty result";
}