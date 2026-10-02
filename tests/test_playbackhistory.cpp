#include <gtest/gtest.h>
#include "../playbackhistory.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

class PlaybackHistoryTest : public ::testing::Test {
protected:
    QString storagePath() const { return QDir(tempDir.path()).filePath("history.json"); }

    QTemporaryDir tempDir;
};

TEST_F(PlaybackHistoryTest, RecordsEligiblePosition) {
    PlaybackHistory history(storagePath());
    history.recordPosition("/videos/a.mp4", 720000, 1800000);
    EXPECT_EQ(history.savedPosition("/videos/a.mp4"), 720000);
    EXPECT_EQ(history.savedPosition("file:///videos/a.mp4"), 720000);
}

TEST_F(PlaybackHistoryTest, IneligiblePositionRemovesEntry) {
    PlaybackHistory history(storagePath());
    history.recordPosition("/videos/a.mp4", 720000, 1800000);
    history.recordPosition("/videos/a.mp4", 1750000, 1800000);
    EXPECT_EQ(history.savedPosition("/videos/a.mp4"), -1);

    history.recordPosition("/videos/b.mp4", 1000, 1800000);
    EXPECT_EQ(history.savedPosition("/videos/b.mp4"), -1);
}

TEST_F(PlaybackHistoryTest, NetworkUrlsNeverStorePositions) {
    PlaybackHistory history(storagePath());
    history.recordPosition("https://example.org/movie.mp4", 720000, 1800000);
    EXPECT_EQ(history.savedPosition("https://example.org/movie.mp4"), -1);
}

TEST_F(PlaybackHistoryTest, EvictsOldestBeyondLimit) {
    PlaybackHistory history(storagePath());
    for (int i = 0; i <= PlaybackHistory::maxPositions; ++i) {
        history.recordPosition(QString("/videos/%1.mp4").arg(i), 50000, 100000);
    }
    EXPECT_EQ(history.savedPosition("/videos/0.mp4"), -1);
    EXPECT_EQ(history.savedPosition("/videos/1.mp4"), 50000);
    EXPECT_EQ(history.savedPosition(QString("/videos/%1.mp4").arg(PlaybackHistory::maxPositions)), 50000);
}

TEST_F(PlaybackHistoryTest, RecentListIsNewestFirstWithoutDuplicates) {
    PlaybackHistory history(storagePath());
    history.recordOpened("/videos/a.mp4");
    history.recordOpened("/videos/b.mp4");
    history.recordOpened("file:///videos/a.mp4");
    EXPECT_EQ(history.recentFiles(), QStringList({"/videos/a.mp4", "/videos/b.mp4"}));
}

TEST_F(PlaybackHistoryTest, RecentListIsCapped) {
    PlaybackHistory history(storagePath());
    for (int i = 0; i < PlaybackHistory::maxRecent + 5; ++i) {
        history.recordOpened(QString("/videos/%1.mp4").arg(i));
    }
    const QStringList recent = history.recentFiles();
    ASSERT_EQ(recent.size(), PlaybackHistory::maxRecent);
    EXPECT_EQ(recent.first(), QString("/videos/%1.mp4").arg(PlaybackHistory::maxRecent + 4));
}

TEST_F(PlaybackHistoryTest, RecentListKeepsStreamUrls) {
    PlaybackHistory history(storagePath());
    history.recordOpened("https://example.org/live.m3u8");
    EXPECT_EQ(history.recentFiles(), QStringList({"https://example.org/live.m3u8"}));
}

TEST_F(PlaybackHistoryTest, RoundTripsThroughDisk) {
    {
        PlaybackHistory history(storagePath());
        history.recordPosition("/videos/a.mp4", 720000, 1800000);
        history.recordOpened("/videos/a.mp4");
        history.recordOpened("/videos/b.mp4");
    }
    PlaybackHistory reloaded(storagePath());
    EXPECT_EQ(reloaded.savedPosition("/videos/a.mp4"), 720000);
    EXPECT_EQ(reloaded.recentFiles(), QStringList({"/videos/b.mp4", "/videos/a.mp4"}));
}

TEST_F(PlaybackHistoryTest, WritesVersionedDocument) {
    {
        PlaybackHistory history(storagePath());
        history.recordOpened("/videos/a.mp4");
    }
    QFile file(storagePath());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    EXPECT_EQ(root.value("version").toInt(), PlaybackHistory::formatVersion);
    EXPECT_TRUE(root.value("positions").isArray());
    EXPECT_TRUE(root.value("recent").isArray());
}

TEST_F(PlaybackHistoryTest, CorruptFileIsMovedAside) {
    {
        QFile file(storagePath());
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write("{not json");
    }
    PlaybackHistory history(storagePath());
    EXPECT_TRUE(history.recentFiles().isEmpty());
    EXPECT_FALSE(QFile::exists(storagePath()));
    EXPECT_TRUE(QFile::exists(storagePath() + ".bak"));
}

TEST_F(PlaybackHistoryTest, UnknownVersionIsMovedAside) {
    {
        QFile file(storagePath());
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(R"({"version": 99, "positions": [], "recent": []})");
    }
    PlaybackHistory history(storagePath());
    EXPECT_TRUE(QFile::exists(storagePath() + ".bak"));
}

TEST_F(PlaybackHistoryTest, ClearEmptiesEverything) {
    PlaybackHistory history(storagePath());
    history.recordPosition("/videos/a.mp4", 720000, 1800000);
    history.recordOpened("/videos/a.mp4");
    history.clear();
    EXPECT_EQ(history.savedPosition("/videos/a.mp4"), -1);
    EXPECT_TRUE(history.recentFiles().isEmpty());
}

TEST_F(PlaybackHistoryTest, PositionKeyIsSha256OfNormalisedPath) {
    const QString key = PlaybackHistory::positionKey("/videos/a.mp4");
    EXPECT_EQ(key.size(), 64);
    EXPECT_EQ(key, PlaybackHistory::positionKey("file:///videos/a.mp4"));
    EXPECT_EQ(key, PlaybackHistory::positionKey("/videos/./a.mp4"));
    EXPECT_NE(key, PlaybackHistory::positionKey("/videos/b.mp4"));
}

TEST_F(PlaybackHistoryTest, PositionsAreStoredWithoutPaths) {
    {
        PlaybackHistory history(storagePath());
        history.recordPosition("/videos/secret-episode.mkv", 720000, 1800000);
    }
    QFile file(storagePath());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QByteArray data = file.readAll();
    EXPECT_FALSE(data.contains("secret-episode"));
    EXPECT_TRUE(data.contains(PlaybackHistory::positionKey("/videos/secret-episode.mkv").toLatin1()));

    PlaybackHistory reloaded(storagePath());
    EXPECT_EQ(reloaded.savedPosition("/videos/secret-episode.mkv"), 720000);
}

TEST_F(PlaybackHistoryTest, MalformedKeysAreDropped) {
    {
        QFile file(storagePath());
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(R"({"version": 1, "positions": [{"key": "not-a-hash", "positionMs": 600000, "durationMs": 1800000}], "recent": []})");
    }
    PlaybackHistory history(storagePath());
    history.recordPosition("/videos/a.mp4", 50000, 100000);
    history.waitForPendingWrites();
    QFile file(storagePath());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(QJsonDocument::fromJson(file.readAll()).object().value("positions").toArray().size(), 1);
}

TEST_F(PlaybackHistoryTest, RemoveRecentDropsOnlyThatEntry) {
    PlaybackHistory history(storagePath());
    history.recordOpened("/videos/a.mp4");
    history.recordOpened("/videos/b.mp4");
    history.removeRecent("/videos/a.mp4");
    EXPECT_EQ(history.recentFiles(), QStringList({"/videos/b.mp4"}));
}
