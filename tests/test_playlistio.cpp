#include <gtest/gtest.h>
#include "../playlistio.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

class PlaylistIOTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(tempDir.isValid());
        QDir(tempDir.path()).mkpath("sub");
    }

    QString touch(const QString &relative) {
        const QString path = QDir(tempDir.path()).filePath(relative);
        QFile file(path);
        EXPECT_TRUE(file.open(QIODevice::WriteOnly));
        return path;
    }

    PlaylistReadResult parse(const QByteArray &data) { return PlaylistIO::parse(data, tempDir.path(), extensions); }

    QTemporaryDir tempDir;
    const QStringList extensions{"mp4", "mkv", "mp3"};
};

TEST_F(PlaylistIOTest, RoundTripsHundredEntries) {
    PlaylistDocument document;
    for (int i = 0; i < 100; ++i) {
        document.entries.append({QUrl::fromLocalFile(touch(QString("item%1.mp4").arg(i))), QString("Item %1").arg(i), i});
    }
    const QString path = QDir(tempDir.path()).filePath("list.m3u8");
    ASSERT_TRUE(PlaylistIO::write(path, document));

    const PlaylistReadResult result = PlaylistIO::read(path, extensions);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.document.entries.size(), 100);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(result.document.entries[i].location, document.entries[i].location);
        EXPECT_EQ(result.document.entries[i].title, document.entries[i].title);
        EXPECT_EQ(result.document.entries[i].durationSec, i);
    }
    EXPECT_EQ(result.skippedMissing, 0);
    EXPECT_EQ(result.document.currentIndex, -1);
}

TEST_F(PlaylistIOTest, WritesExtendedHeaderWithoutBom) {
    PlaylistDocument document;
    document.entries.append({QUrl("https://example.org/live.m3u8"), "Radio", -1});
    const QByteArray data = PlaylistIO::serialise(document);
    EXPECT_TRUE(data.startsWith("#EXTM3U\n"));
    EXPECT_TRUE(data.contains("#EXTINF:-1,Radio\nhttps://example.org/live.m3u8\n"));
    EXPECT_FALSE(data.contains("#GAV-CURRENT"));
}

TEST_F(PlaylistIOTest, ResolvesRelativePaths) {
    const QString a = touch("a.mp4");
    const QString b = touch("sub/b.mkv");
    const PlaylistReadResult result = parse("#EXTM3U\na.mp4\n./sub/b.mkv\nsub\\b.mkv\n");
    ASSERT_EQ(result.document.entries.size(), 3);
    EXPECT_EQ(result.document.entries[0].location, QUrl::fromLocalFile(a));
    EXPECT_EQ(result.document.entries[1].location, QUrl::fromLocalFile(b));
    EXPECT_EQ(result.document.entries[2].location, QUrl::fromLocalFile(b));
}

TEST_F(PlaylistIOTest, ToleratesBomCrlfAndMissingHeader) {
    touch("a.mp4");
    const PlaylistReadResult result = parse("\xEF\xBB\xBF#EXTINF:12,Title A\r\na.mp4\r\n");
    ASSERT_EQ(result.document.entries.size(), 1);
    EXPECT_EQ(result.document.entries[0].title, "Title A");
    EXPECT_EQ(result.document.entries[0].durationSec, 12);
}

TEST_F(PlaylistIOTest, AcceptsFileAndHttpUrls) {
    const QString a = touch("a.mp4");
    const QByteArray data = "#EXTM3U\n" + QUrl::fromLocalFile(a).toEncoded() + "\nhttps://example.org/stream\nhttp://example.org/x.mp4\n";
    const PlaylistReadResult result = parse(data);
    ASSERT_EQ(result.document.entries.size(), 3);
    EXPECT_EQ(result.document.entries[0].location, QUrl::fromLocalFile(a));
    EXPECT_EQ(result.document.entries[1].location, QUrl("https://example.org/stream"));
    EXPECT_EQ(result.document.entries[2].location, QUrl("http://example.org/x.mp4"));
}

TEST_F(PlaylistIOTest, CountsUnsupportedSchemesAndExtensions) {
    touch("notes.txt");
    const PlaylistReadResult result = parse("ftp://example.org/a.mp4\nrtsp://example.org/live\nnotes.txt\n");
    EXPECT_TRUE(result.document.entries.isEmpty());
    EXPECT_EQ(result.skippedUnsupported, 3);
    EXPECT_EQ(result.skippedMissing, 0);
}

TEST_F(PlaylistIOTest, CountsMissingFiles) {
    touch("a.mp4");
    const PlaylistReadResult result = parse("a.mp4\nmissing.mp4\n");
    EXPECT_EQ(result.document.entries.size(), 1);
    EXPECT_EQ(result.skippedMissing, 1);
}

TEST_F(PlaylistIOTest, ExtinfAppliesOnlyToNextEntry) {
    touch("a.mp4");
    touch("b.mp4");
    const PlaylistReadResult result = parse("#EXTINF:5,First\na.mp4\nb.mp4\n");
    ASSERT_EQ(result.document.entries.size(), 2);
    EXPECT_EQ(result.document.entries[1].title, "");
    EXPECT_EQ(result.document.entries[1].durationSec, -1);
}

TEST_F(PlaylistIOTest, CurrentIndexRoundTrips) {
    PlaylistDocument document;
    document.entries.append({QUrl::fromLocalFile(touch("a.mp4")), "A", -1});
    document.entries.append({QUrl::fromLocalFile(touch("b.mp4")), "B", -1});
    document.currentIndex = 1;
    const PlaylistReadResult result = parse(PlaylistIO::serialise(document));
    EXPECT_EQ(result.document.currentIndex, 1);
}

TEST_F(PlaylistIOTest, CurrentIndexShiftsPastSkippedEntries) {
    touch("b.mp4");
    touch("c.mp4");
    const PlaylistReadResult result = parse("#EXTM3U\n#GAV-CURRENT:2\nmissing.mp4\nb.mp4\nc.mp4\n");
    ASSERT_EQ(result.document.entries.size(), 2);
    EXPECT_EQ(result.document.currentIndex, 1);
}

TEST_F(PlaylistIOTest, CurrentIndexMovesToNextSurvivorWhenItsEntryIsMissing) {
    touch("a.mp4");
    touch("c.mp4");
    const PlaylistReadResult result = parse("#GAV-CURRENT:1\na.mp4\nmissing.mp4\nc.mp4\n");
    EXPECT_EQ(result.document.currentIndex, 1);
}

TEST_F(PlaylistIOTest, CurrentIndexClampsWhenTrailingEntriesAreMissing) {
    touch("a.mp4");
    const PlaylistReadResult result = parse("#GAV-CURRENT:2\na.mp4\nmissing1.mp4\nmissing2.mp4\n");
    EXPECT_EQ(result.document.currentIndex, 0);
}

TEST_F(PlaylistIOTest, CurrentIndexClearedWhenNothingSurvives) {
    const PlaylistReadResult result = parse("#GAV-CURRENT:0\nmissing.mp4\n");
    EXPECT_TRUE(result.document.entries.isEmpty());
    EXPECT_EQ(result.document.currentIndex, -1);
}

TEST_F(PlaylistIOTest, ReadsFixtureWithRelativeAndMissingEntries) {
    const PlaylistReadResult result = PlaylistIO::read(QStringLiteral(GAV_TEST_DATA_DIR "/playlist-relative.m3u8"), extensions);
    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.document.entries.size(), 2);
    EXPECT_EQ(result.skippedMissing, 1);
}

TEST_F(PlaylistIOTest, ReadReportsUnreadableFile) {
    const PlaylistReadResult result = PlaylistIO::read(QDir(tempDir.path()).filePath("nope.m3u8"), extensions);
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.isEmpty());
}

TEST_F(PlaylistIOTest, VariantConversionPrefersTitleOverName) {
    PlaylistReadResult read;
    read.ok = true;
    read.document.entries.append({QUrl("https://example.org/live"), "Radio", -1});
    const QVariantList entries = PlaylistFiles::toVariant(read).value("entries").toList();
    const PlaylistDocument document = PlaylistFiles::fromVariant(entries, -1);
    ASSERT_EQ(document.entries.size(), 1);
    EXPECT_EQ(document.entries[0].title, "Radio");
}

TEST_F(PlaylistIOTest, VariantConversionKeepsOrderAndCurrentIndex) {
    const QVariantList items{QVariantMap{{"name", "A"}, {"path", "file:///videos/a.mp4"}},
                             QVariantMap{{"name", "Live"}, {"path", "https://example.org/live"}}};
    const PlaylistDocument document = PlaylistFiles::fromVariant(items, 1);
    ASSERT_EQ(document.entries.size(), 2);
    EXPECT_EQ(document.entries[0].title, "A");
    EXPECT_EQ(document.entries[1].location, QUrl("https://example.org/live"));
    EXPECT_EQ(document.currentIndex, 1);
}
