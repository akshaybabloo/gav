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
    bool waitForSignal(QSignalSpy& spy, int timeoutMs = 5000) {
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(collage, &Collage::collageFinished, &loop, &QEventLoop::quit);
        
        timer.start(timeoutMs);
        loop.exec();
        
        return spy.count() > 0;
    }

    Collage* collage;
    QTemporaryDir* tempDir;
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
    QList<QUrl> invalidPaths;
    invalidPaths.append(QUrl("file:///nonexistent/video.mp4"));
    
    QSignalSpy startedSpy(collage, &Collage::collageStarted);
    QSignalSpy finishedSpy(collage, &Collage::collageFinished);
    
    collage->toCollage(invalidPaths);
    
    // Wait for async operation to complete (with timeout)
    ASSERT_TRUE(waitForSignal(finishedSpy, 10000)) << "Timed out waiting for collageFinished signal";
    
    // Should emit started with count 1
    ASSERT_EQ(startedSpy.count(), 1);
    EXPECT_EQ(startedSpy.takeFirst().at(0).toInt(), 1);
    
    // Should emit finished (expecting failure for invalid file)
    ASSERT_EQ(finishedSpy.count(), 1);
    QList<QVariant> arguments = finishedSpy.takeFirst();
    int successCount = arguments.at(0).toInt();
    int failCount = arguments.at(1).toInt();
    
    // Invalid file should fail
    EXPECT_EQ(successCount, 0);
    EXPECT_EQ(failCount, 1);
}


