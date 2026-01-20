#include <gtest/gtest.h>
#include <QUrl>
#include <QFileInfo>
#include <QDir>

// Test path conversion logic similar to what's in main.cpp
class PathConversionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for testing
        tempDir = QDir::temp().filePath("gav_test");
        QDir().mkpath(tempDir);
    }

    void TearDown() override {
        // Clean up
        QDir(tempDir).removeRecursively();
    }

    QString tempDir;
};

TEST_F(PathConversionTest, RelativePathConversion) {
    QString relativePath = "./test.mp4";
    QFileInfo fileInfo(relativePath);

    EXPECT_TRUE(fileInfo.isRelative());

    QString absolutePath = QDir::current().absoluteFilePath(relativePath);
    QUrl url = QUrl::fromLocalFile(absolutePath);

    EXPECT_TRUE(url.isValid());
    EXPECT_TRUE(url.isLocalFile());
}

TEST_F(PathConversionTest, AbsolutePathConversion) {
    QString absolutePath = "/home/user/videos/test.mp4";
    QFileInfo fileInfo(absolutePath);

    EXPECT_TRUE(fileInfo.isAbsolute());

    QUrl url = QUrl::fromLocalFile(absolutePath);

    EXPECT_TRUE(url.isValid());
    EXPECT_TRUE(url.isLocalFile());
    EXPECT_EQ(url.toLocalFile(), absolutePath);
}

TEST_F(PathConversionTest, URLParsing) {
    QString urlString = "file:///home/user/videos/test.mp4";
    QUrl url = QUrl::fromUserInput(urlString);

    EXPECT_TRUE(url.isValid());
    EXPECT_TRUE(url.isLocalFile());
}

TEST_F(PathConversionTest, InvalidPath) {
    QString invalidPath = "";
    QUrl url = QUrl::fromUserInput(invalidPath);

    EXPECT_TRUE(url.isEmpty());
}

TEST_F(PathConversionTest, PathWithSpaces) {
    QString pathWithSpaces = "/home/user/my videos/test file.mp4";
    QUrl url = QUrl::fromLocalFile(pathWithSpaces);

    EXPECT_TRUE(url.isValid());
    EXPECT_TRUE(url.isLocalFile());
}