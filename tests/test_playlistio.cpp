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
    EXPECT_EQ(result.unavailable, 0);
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

#ifndef Q_OS_WIN
TEST_F(PlaylistIOTest, KeepsLiteralBackslashInExistingPosixFilename) {
    const QString literal = touch("part\\one.mp4");
    const PlaylistReadResult result = parse("part\\one.mp4\n");
    ASSERT_EQ(result.document.entries.size(), 1);
    EXPECT_EQ(result.document.entries[0].location, QUrl::fromLocalFile(literal));
}
#endif

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
    EXPECT_EQ(result.unavailable, 0);
}

TEST_F(PlaylistIOTest, KeepsMissingFilesAsUnavailable) {
    touch("a.mp4");
    const PlaylistReadResult result = parse("a.mp4\n#EXTINF:10,Gone\nmissing.mp4\n");
    ASSERT_EQ(result.document.entries.size(), 2);
    EXPECT_TRUE(result.document.entries[0].available);
    EXPECT_TRUE(result.document.entries[0].reason.isEmpty());
    EXPECT_FALSE(result.document.entries[1].available);
    EXPECT_EQ(result.document.entries[1].reason, "File not found");
    EXPECT_EQ(result.document.entries[1].title, "Gone");
    EXPECT_EQ(result.document.entries[1].location, QUrl::fromLocalFile(QDir(tempDir.path()).filePath("missing.mp4")));
    EXPECT_EQ(result.unavailable, 1);
    EXPECT_EQ(result.skippedUnsupported, 0);
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
    const PlaylistReadResult result = parse("#EXTM3U\n#GAV-CURRENT:2\nnotes.txt\nb.mp4\nc.mp4\n");
    ASSERT_EQ(result.document.entries.size(), 2);
    EXPECT_EQ(result.document.currentIndex, 1);
}

TEST_F(PlaylistIOTest, CurrentIndexMovesToNextSurvivorWhenItsEntryIsSkipped) {
    touch("a.mp4");
    touch("c.mp4");
    const PlaylistReadResult result = parse("#GAV-CURRENT:1\na.mp4\nnotes.txt\nc.mp4\n");
    ASSERT_EQ(result.document.entries.size(), 2);
    EXPECT_EQ(result.document.currentIndex, 1);
}

TEST_F(PlaylistIOTest, CurrentIndexClampsWhenTrailingEntriesAreSkipped) {
    touch("a.mp4");
    const PlaylistReadResult result = parse("#GAV-CURRENT:2\na.mp4\none.txt\ntwo.txt\n");
    EXPECT_EQ(result.document.currentIndex, 0);
}

TEST_F(PlaylistIOTest, CurrentIndexClearedWhenNothingSurvives) {
    const PlaylistReadResult result = parse("#GAV-CURRENT:0\nnotes.txt\n");
    EXPECT_TRUE(result.document.entries.isEmpty());
    EXPECT_EQ(result.document.currentIndex, -1);
}

TEST_F(PlaylistIOTest, CurrentIndexStaysOnAMissingEntry) {
    touch("a.mp4");
    touch("c.mp4");
    const PlaylistReadResult result = parse("#GAV-CURRENT:1\na.mp4\nmissing.mp4\nc.mp4\n");
    ASSERT_EQ(result.document.entries.size(), 3);
    EXPECT_EQ(result.document.currentIndex, 1);
}

TEST_F(PlaylistIOTest, ReadsFixtureWithRelativeAndMissingEntries) {
    const PlaylistReadResult result = PlaylistIO::read(QStringLiteral(GAV_TEST_DATA_DIR "/playlist-relative.m3u8"), extensions);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.document.entries.size(), 3);
    EXPECT_FALSE(result.document.entries[2].available);
    EXPECT_EQ(result.unavailable, 1);
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

TEST_F(PlaylistIOTest, VariantConversionRemapsCurrentIndexPastInvalidItems) {
    const QVariantList items{QVariantMap{{"name", "Broken"}, {"path", ""}},
                             QVariantMap{{"name", "A"}, {"path", "file:///videos/a.mp4"}},
                             QVariantMap{{"name", "B"}, {"path", "file:///videos/b.mp4"}}};
    EXPECT_EQ(PlaylistFiles::fromVariant(items, 2).currentIndex, 1);
    EXPECT_EQ(PlaylistFiles::fromVariant(items, 0).currentIndex, -1);
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

TEST_F(PlaylistIOTest, ExtinfTitleFollowsQuotedAttributes) {
    const PlaylistReadResult result = parse("#EXTM3U\n"
                                            "#EXTINF:-1 tvg-id=\"One.ua\" http-user-agent=\"Mozilla/5.0 (KHTML, like Gecko)\" group-title=\"General\",1+1, International\n"
                                            "http://example.org/one.m3u8\n"
                                            "#EXTINF:9.6,Fractional\n"
                                            "http://example.org/two.ts\n");
    ASSERT_EQ(result.document.entries.size(), 2);
    EXPECT_EQ(result.document.entries[0].title, "1+1, International");
    EXPECT_EQ(result.document.entries[0].durationSec, -1);
    EXPECT_EQ(result.document.entries[1].title, "Fractional");
    EXPECT_EQ(result.document.entries[1].durationSec, 10);
}

TEST_F(PlaylistIOTest, RemotePlaylistResolvesRelativeEntriesAndSkipsLocalOnes) {
    const QString local = touch("a.mp4");
    const QByteArray data = "#EXTM3U\r\n"
                            "#EXTINF:-1,Relative\r\n"
                            "channels/one.m3u8\r\n"
                            "#EXTINF:-1,Absolute\r\n"
                            "https://other.example/two.mp4\r\n"
                            "/shared/three.mp4\r\n"
                            "C:/Videos/four.mp4\r\n"
                            "C:\\Videos\\five.mp4\r\n" +
                            QUrl::fromLocalFile(local).toString().toUtf8() + "\r\nrtsp://example.org/six\r\n";
    const PlaylistReadResult result =
        PlaylistIO::parseRemote(data, QUrl("https://example.org/lists/index.m3u"), QUrl("https://cdn.example.org/lists/index.m3u"));
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.document.entries.size(), 3);
    EXPECT_EQ(result.document.entries[0].location, QUrl("https://cdn.example.org/lists/channels/one.m3u8"));
    EXPECT_EQ(result.document.entries[0].title, "Relative");
    EXPECT_EQ(result.document.entries[1].location, QUrl("https://other.example/two.mp4"));
    EXPECT_EQ(result.document.entries[2].location, QUrl("https://cdn.example.org/shared/three.mp4"));
    EXPECT_EQ(result.skippedUnsupported, 4);
    EXPECT_EQ(result.unavailable, 0);
}

TEST_F(PlaylistIOTest, RemoteHlsPlaylistBecomesOneStreamEntry) {
    const QUrl source("https://example.org/live/master.m3u8");
    const QByteArray master = "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=288000\nlow/index.m3u8\n";
    const QByteArray media = "#EXTM3U\r\n#EXT-X-TARGETDURATION:10\r\n#EXTINF:9.6,\r\nsegment0.ts\r\n";
    for (const QByteArray &data : {master, media}) {
        EXPECT_TRUE(PlaylistIO::isHlsPlaylist(data));
        const PlaylistReadResult result = PlaylistIO::parseRemote(data, source, QUrl("https://cdn.example.org/live/master.m3u8"));
        ASSERT_TRUE(result.ok);
        ASSERT_EQ(result.document.entries.size(), 1);
        EXPECT_EQ(result.document.entries[0].location, source);
    }
    EXPECT_FALSE(PlaylistIO::isHlsPlaylist("#EXTM3U\n#EXTINF:-1,Channel\nhttp://example.org/a.m3u8\n"));
}

TEST_F(PlaylistIOTest, RemoteBodyWithoutPlaylistMarkersHasNoEntries) {
    const QUrl source("https://example.org/lists/index.m3u");
    for (const QByteArray &body : {QByteArray("<html>\n<body>Not found</body>\n</html>\n"), QByteArray("{\"error\":\"denied\"}\n"), QByteArray("Not found\n")}) {
        const PlaylistReadResult result = PlaylistIO::parseRemote(body, source, source);
        EXPECT_TRUE(result.document.entries.isEmpty());
        EXPECT_EQ(result.skippedUnsupported, 0);
    }
    const PlaylistReadResult headerless = PlaylistIO::parseRemote("#EXTINF:-1,Channel\nlive/one.m3u8\n", source, source);
    ASSERT_EQ(headerless.document.entries.size(), 1);
    EXPECT_EQ(headerless.document.entries[0].title, "Channel");
}

TEST_F(PlaylistIOTest, ReadsGroupAndLogoAttributes) {
    const PlaylistReadResult result = parse("#EXTM3U\n"
                                            "#EXTINF:-1 tvg-id=\"News.uk\" tvg-logo=\"https://example.org/news.png\" group-title=\"News, World\",News Channel\n"
                                            "https://example.org/news.m3u8\n"
                                            "#EXTINF:-1 TVG-LOGO=http://example.org/sport.png Group-Title=Sport tvg-name=\"Sport 1\",Sport\n"
                                            "https://example.org/sport.m3u8\n"
                                            "#EXTINF:120,Plain\n"
                                            "https://example.org/plain.mp4\n");
    ASSERT_EQ(result.document.entries.size(), 3);
    EXPECT_EQ(result.document.entries[0].title, "News Channel");
    EXPECT_EQ(result.document.entries[0].group, "News, World");
    EXPECT_EQ(result.document.entries[0].logo, QUrl("https://example.org/news.png"));
    EXPECT_EQ(result.document.entries[1].title, "Sport");
    EXPECT_EQ(result.document.entries[1].group, "Sport");
    EXPECT_EQ(result.document.entries[1].logo, QUrl("http://example.org/sport.png"));
    EXPECT_TRUE(result.document.entries[2].group.isEmpty());
    EXPECT_TRUE(result.document.entries[2].logo.isEmpty());
    EXPECT_TRUE(result.document.entries[2].attributes.isEmpty());
    EXPECT_EQ(result.document.entries[2].durationSec, 120);
}

TEST_F(PlaylistIOTest, KeepsAttributeTextVerbatim) {
    const QByteArray attributes = "tvg-id=\"One.ua\" http-user-agent=\"Mozilla/5.0 (KHTML, like Gecko)\" catchup=append group-title=\"General\"";
    const PlaylistReadResult result = parse("#EXTINF:-1 " + attributes + ",1+1, International\nhttp://example.org/one.m3u8\n");
    ASSERT_EQ(result.document.entries.size(), 1);
    EXPECT_EQ(result.document.entries[0].attributes, QString::fromUtf8(attributes));
    EXPECT_EQ(result.document.entries[0].title, "1+1, International");
    EXPECT_EQ(result.document.entries[0].group, "General");
}

TEST_F(PlaylistIOTest, ExtgrpSetsGroupOnlyWithoutGroupTitle) {
    const PlaylistReadResult result = parse("#EXTGRP:Samples\n"
                                            "#EXTINF:-1,From EXTGRP\n"
                                            "https://example.org/a.m3u8\n"
                                            "#EXTINF:-1 group-title=\"Films\",From attribute\n"
                                            "#EXTGRP:Ignored\n"
                                            "https://example.org/b.m3u8\n"
                                            "#EXTINF:-1,No group\n"
                                            "https://example.org/c.m3u8\n");
    ASSERT_EQ(result.document.entries.size(), 3);
    EXPECT_EQ(result.document.entries[0].group, "Samples");
    EXPECT_EQ(result.document.entries[1].group, "Films");
    EXPECT_TRUE(result.document.entries[2].group.isEmpty());
}

TEST_F(PlaylistIOTest, IgnoresLogoThatIsNotAWebAddress) {
    const PlaylistReadResult result = parse("#EXTINF:-1 tvg-logo=\"file:///etc/passwd\",Local logo\n"
                                            "https://example.org/a.m3u8\n"
                                            "#EXTINF:-1 tvg-logo=\"logos/b.png\",Relative logo\n"
                                            "https://example.org/b.m3u8\n");
    ASSERT_EQ(result.document.entries.size(), 2);
    EXPECT_TRUE(result.document.entries[0].logo.isEmpty());
    EXPECT_EQ(result.document.entries[0].attributes, "tvg-logo=\"file:///etc/passwd\"");
    EXPECT_TRUE(result.document.entries[1].logo.isEmpty());
    EXPECT_EQ(result.document.entries[1].attributes, "tvg-logo=\"logos/b.png\"");
}

TEST_F(PlaylistIOTest, SerialiseWritesAttributesAndGroup) {
    PlaylistDocument document;
    PlaylistEntry kept{QUrl("https://example.org/a.m3u8"), "Kept", -1};
    kept.attributes = "tvg-id=\"A.uk\" group-title=\"News\" tvg-name=\"A\"";
    kept.group = "News";
    PlaylistEntry added{QUrl("https://example.org/b.m3u8"), "Added", 600};
    added.attributes = "tvg-id=\"B.uk\"";
    added.group = "Films";
    PlaylistEntry replaced{QUrl("https://example.org/c.m3u8"), "Replaced", -1};
    replaced.attributes = "GROUP-TITLE=Old tvg-id=\"C.uk\"";
    replaced.group = "The \"Best\" Films";
    PlaylistEntry bare{QUrl("https://example.org/d.m3u8"), "Bare", -1};
    bare.group = "Samples";
    document.entries = {kept, added, replaced, bare};

    const QByteArray data = PlaylistIO::serialise(document);
    EXPECT_TRUE(data.contains("#EXTINF:-1 tvg-id=\"A.uk\" group-title=\"News\" tvg-name=\"A\",Kept\n"));
    EXPECT_TRUE(data.contains("#EXTINF:600 tvg-id=\"B.uk\" group-title=\"Films\",Added\n"));
    EXPECT_TRUE(data.contains("#EXTINF:-1 group-title=\"The 'Best' Films\" tvg-id=\"C.uk\",Replaced\n"));
    EXPECT_TRUE(data.contains("#EXTINF:-1 group-title=\"Samples\",Bare\n"));

    const PlaylistReadResult result = parse(data);
    ASSERT_EQ(result.document.entries.size(), 4);
    EXPECT_EQ(result.document.entries[2].group, "The 'Best' Films");
    EXPECT_EQ(result.document.entries[2].title, "Replaced");
}

TEST_F(PlaylistIOTest, RoundTripsMixedFixture) {
    const PlaylistReadResult first = PlaylistIO::read(QStringLiteral(GAV_TEST_DATA_DIR "/mixed.m3u8"), extensions);
    ASSERT_TRUE(first.ok);
    ASSERT_EQ(first.document.entries.size(), 8);
    EXPECT_EQ(first.unavailable, 1);
    EXPECT_EQ(first.document.entries[2].group, "Films");
    EXPECT_EQ(first.document.entries[3].logo, QUrl("https://example.org/news.png"));
    EXPECT_EQ(first.document.entries[3].attributes, "tvg-id=\"News.uk\" tvg-logo=\"https://example.org/news.png\" group-title=\"News\"");
    EXPECT_FALSE(first.document.entries[4].available);
    EXPECT_EQ(first.document.entries[6].group, "Samples");

    const QString path = QDir(tempDir.path()).filePath("mixed-copy.m3u8");
    ASSERT_TRUE(PlaylistIO::write(path, first.document));
    const PlaylistReadResult second = PlaylistIO::read(path, extensions);
    ASSERT_TRUE(second.ok);
    ASSERT_EQ(second.document.entries.size(), first.document.entries.size());
    for (qsizetype i = 0; i < first.document.entries.size(); ++i) {
        const PlaylistEntry &before = first.document.entries[i];
        const PlaylistEntry &after = second.document.entries[i];
        EXPECT_EQ(after.location, before.location) << i;
        EXPECT_EQ(after.title, before.title) << i;
        EXPECT_EQ(after.durationSec, before.durationSec) << i;
        EXPECT_EQ(after.group, before.group) << i;
        EXPECT_EQ(after.available, before.available) << i;
        if (i == 6) {
            EXPECT_EQ(after.attributes, "group-title=\"Samples\"");
        } else {
            EXPECT_EQ(after.attributes, before.attributes) << i;
        }
    }
    EXPECT_EQ(PlaylistIO::serialise(second.document), PlaylistIO::serialise(first.document));
}

TEST_F(PlaylistIOTest, VariantConversionCarriesPlaylistFields) {
    PlaylistReadResult read;
    read.ok = true;
    read.unavailable = 1;
    PlaylistEntry news{QUrl("https://example.org/news.m3u8"), "News", -1};
    news.group = "News";
    news.logo = QUrl("https://example.org/news.png");
    news.attributes = "tvg-id=\"News.uk\" group-title=\"News\"";
    PlaylistEntry gone{QUrl("file:///videos/gone.mp4"), "Gone", 30};
    gone.available = false;
    gone.reason = "File not found";
    read.document.entries = {news, gone};

    const QVariantMap map = PlaylistFiles::toVariant(read);
    EXPECT_EQ(map.value("unavailable").toInt(), 1);
    EXPECT_FALSE(map.contains("skippedMissing"));
    const QVariantList entries = map.value("entries").toList();
    ASSERT_EQ(entries.size(), 2);
    EXPECT_EQ(entries[0].toMap().value("group").toString(), "News");
    EXPECT_EQ(entries[0].toMap().value("logo").toString(), "https://example.org/news.png");
    EXPECT_TRUE(entries[0].toMap().value("available").toBool());
    EXPECT_FALSE(entries[1].toMap().value("available").toBool());
    EXPECT_EQ(entries[1].toMap().value("reason").toString(), "File not found");

    const PlaylistDocument document = PlaylistFiles::fromVariant(entries, -1);
    ASSERT_EQ(document.entries.size(), 2);
    EXPECT_EQ(document.entries[0].group, "News");
    EXPECT_EQ(document.entries[0].logo, QUrl("https://example.org/news.png"));
    EXPECT_EQ(document.entries[0].attributes, news.attributes);
    EXPECT_FALSE(document.entries[1].available);
    EXPECT_EQ(document.entries[1].reason, "File not found");
    EXPECT_EQ(document.entries[1].durationSec, 30);
}
