#include <gtest/gtest.h>
#include "../playlistmodel.h"

#include <QAbstractItemModelTester>
#include <QSet>
#include <QSignalSpy>

namespace {

QVariantMap item(const QString &path, const QString &title = QString()) {
    QVariantMap map{{"path", path}};
    if (!title.isEmpty()) {
        map.insert("title", title);
    }
    return map;
}

QVariantList files(int count, int first = 0) {
    QVariantList list;
    for (int i = 0; i < count; ++i) {
        list.append(item(QStringLiteral("file:///videos/%1.mp4").arg(first + i)));
    }
    return list;
}

QStringList titles(const PlaylistModel &model) {
    QStringList list;
    for (int row = 0; row < model.count(); ++row) {
        list.append(model.entryAt(row).value("title").toString());
    }
    return list;
}

}

TEST(PlaylistModelTest, AppendInsertsOneBatch) {
    PlaylistModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy counted(&model, &PlaylistModel::countChanged);

    EXPECT_EQ(model.append(files(100)), 0);
    ASSERT_EQ(inserted.size(), 1);
    EXPECT_EQ(inserted[0][1].toInt(), 0);
    EXPECT_EQ(inserted[0][2].toInt(), 99);
    EXPECT_EQ(counted.size(), 1);
    EXPECT_EQ(model.count(), 100);

    EXPECT_EQ(model.append(files(5, 100)), 100);
    EXPECT_EQ(inserted.size(), 2);
    EXPECT_EQ(model.append({}), -1);
    EXPECT_EQ(inserted.size(), 2);
}

TEST(PlaylistModelTest, InsertPlacesEntriesBeforeTheRow) {
    PlaylistModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append({item("file:///a.mp4"), item("file:///d.mp4")});
    EXPECT_EQ(model.insert(1, {item("file:///b.mp4"), item("file:///c.mp4")}), 1);
    EXPECT_EQ(titles(model), QStringList({"a.mp4", "b.mp4", "c.mp4", "d.mp4"}));
    EXPECT_EQ(model.insert(99, {item("file:///e.mp4")}), 4);
    EXPECT_EQ(model.insert(-3, {item("file:///0.mp4")}), 0);
    EXPECT_EQ(titles(model).first(), "0.mp4");
}

TEST(PlaylistModelTest, IdsAreDistinctAndNeverReused) {
    PlaylistModel model;
    model.append(files(3));
    QSet<int> seen;
    for (int row = 0; row < 3; ++row) {
        seen.insert(model.idAt(row));
    }
    EXPECT_EQ(seen.size(), 3);
    model.clear();
    model.append(files(3));
    for (int row = 0; row < 3; ++row) {
        EXPECT_FALSE(seen.contains(model.idAt(row)));
    }
    EXPECT_EQ(model.rowForId(model.idAt(2)), 2);
    EXPECT_EQ(model.rowForId(-1), -1);
    EXPECT_EQ(model.idAt(7), -1);
}

TEST(PlaylistModelTest, TitleIsNeverEmpty) {
    PlaylistModel model;
    model.append({item("file:///videos/The Movie.mkv"), item("https://example.org/live/stream.m3u8?token=1"), item("https://example.org/"),
                  item("file:///videos/x.mp4", "Named"), item("file:///videos/y.mp4", "   ")});
    EXPECT_EQ(titles(model), QStringList({"The Movie.mkv", "stream.m3u8", "https://example.org/", "Named", "y.mp4"}));
}

TEST(PlaylistModelTest, KindComesFromTheLocation) {
    PlaylistModel model;
    model.append({item("file:///a.MP3"), item("file:///a.mkv"), item("file:///a.unknown"), item("http://example.org/a.mp3"), item("/plain/path/song.flac")});
    EXPECT_EQ(model.entryAt(0).value("kind").toInt(), PlaylistModel::LocalAudio);
    EXPECT_EQ(model.entryAt(1).value("kind").toInt(), PlaylistModel::LocalVideo);
    EXPECT_EQ(model.entryAt(2).value("kind").toInt(), PlaylistModel::LocalVideo);
    EXPECT_EQ(model.entryAt(3).value("kind").toInt(), PlaylistModel::Stream);
    EXPECT_EQ(model.entryAt(4).value("kind").toInt(), PlaylistModel::LocalAudio);
    EXPECT_EQ(model.entryAt(4).value("path").toString(), "file:///plain/path/song.flac");

    model.setAudioExtensions({"MKV"});
    model.append({item("file:///b.mkv"), item("file:///b.mp3")});
    EXPECT_EQ(model.entryAt(5).value("kind").toInt(), PlaylistModel::LocalAudio);
    EXPECT_EQ(model.entryAt(6).value("kind").toInt(), PlaylistModel::LocalVideo);
}

TEST(PlaylistModelTest, RejectsLocationsThatAreNeitherLocalNorHttp) {
    PlaylistModel model;
    EXPECT_EQ(model.append({item("rtsp://example.org/a"), item("ftp://example.org/a.mp4"), item(""), QVariantMap()}), -1);
    EXPECT_EQ(model.count(), 0);
    EXPECT_EQ(model.append({item("rtsp://example.org/a"), item("https://example.org/a.mp4")}), 0);
    EXPECT_EQ(model.count(), 1);
}

TEST(PlaylistModelTest, CarriesPlaylistFields) {
    PlaylistModel model;
    model.append({QVariantMap{{"path", "https://example.org/news.m3u8"},
                              {"title", "News"},
                              {"durationSec", 90},
                              {"group", "News"},
                              {"logo", "https://example.org/news.png"},
                              {"attributes", "tvg-id=\"News.uk\""}},
                  QVariantMap{{"path", "file:///missing.mp4"}, {"logo", "file:///etc/passwd"}, {"available", false}, {"reason", "File not found"}}});
    const QVariantMap news = model.entryAt(0);
    EXPECT_EQ(news.value("durationMs").toLongLong(), 90000);
    EXPECT_EQ(news.value("durationSec").toInt(), 90);
    EXPECT_EQ(news.value("group").toString(), "News");
    EXPECT_EQ(news.value("logo").toString(), "https://example.org/news.png");
    EXPECT_EQ(news.value("attributes").toString(), "tvg-id=\"News.uk\"");
    EXPECT_TRUE(news.value("available").toBool());

    const QVariantMap missing = model.entryAt(1);
    EXPECT_EQ(missing.value("durationSec").toInt(), -1);
    EXPECT_TRUE(missing.value("logo").toString().isEmpty());
    EXPECT_FALSE(missing.value("available").toBool());
    EXPECT_EQ(missing.value("reason").toString(), "File not found");
    EXPECT_FALSE(model.data(model.index(1), PlaylistModel::AvailableRole).toBool());
}

TEST(PlaylistModelTest, CurrentEntrySurvivesInsertsAndFollowsItsRow) {
    PlaylistModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append(files(3));
    QSignalSpy current(&model, &PlaylistModel::currentChanged);
    QSignalSpy currentRow(&model, &PlaylistModel::currentRowChanged);

    model.setCurrentRow(1);
    const int id = model.currentId();
    EXPECT_EQ(current.size(), 1);
    EXPECT_EQ(currentRow.size(), 1);
    EXPECT_TRUE(model.data(model.index(1), PlaylistModel::IsCurrentRole).toBool());
    EXPECT_FALSE(model.data(model.index(0), PlaylistModel::IsCurrentRole).toBool());

    model.setCurrentRow(1);
    EXPECT_EQ(current.size(), 1);

    model.append(files(2, 10));
    EXPECT_EQ(model.currentRow(), 1);
    EXPECT_EQ(currentRow.size(), 1);

    model.insert(0, files(2, 20));
    EXPECT_EQ(model.currentRow(), 3);
    EXPECT_EQ(model.currentId(), id);
    EXPECT_EQ(current.size(), 1);
    EXPECT_EQ(currentRow.size(), 2);

    model.setCurrentRow(-1);
    EXPECT_EQ(model.currentId(), -1);
    EXPECT_EQ(current.size(), 2);
}

TEST(PlaylistModelTest, RemovingEntriesKeepsOrClearsTheCurrentEntry) {
    PlaylistModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append(files(5));
    model.setCurrentRow(3);
    const int id = model.currentId();
    QSignalSpy current(&model, &PlaylistModel::currentChanged);

    EXPECT_EQ(model.remove({model.idAt(0), model.idAt(4), 9999}), 2);
    EXPECT_EQ(titles(model), QStringList({"1.mp4", "2.mp4", "3.mp4"}));
    EXPECT_EQ(model.currentRow(), 2);
    EXPECT_EQ(model.currentId(), id);
    EXPECT_EQ(current.size(), 0);
    EXPECT_EQ(model.anchorRow(), -1);

    EXPECT_EQ(model.remove({id, model.idAt(0)}), 2);
    EXPECT_EQ(titles(model), QStringList({"2.mp4"}));
    EXPECT_EQ(model.currentRow(), -1);
    EXPECT_EQ(model.currentId(), -1);
    EXPECT_EQ(current.size(), 1);
    EXPECT_EQ(model.anchorRow(), 1);

    EXPECT_EQ(model.remove({}), 0);
}

TEST(PlaylistModelTest, RemovesNeighbouringRowsInOneNotification) {
    PlaylistModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append(files(10));
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);

    EXPECT_EQ(model.remove({model.idAt(3), model.idAt(8), model.idAt(2), model.idAt(4), model.idAt(7)}), 5);
    ASSERT_EQ(removed.size(), 2);
    EXPECT_EQ(removed[0][1].toInt(), 7);
    EXPECT_EQ(removed[0][2].toInt(), 8);
    EXPECT_EQ(removed[1][1].toInt(), 2);
    EXPECT_EQ(removed[1][2].toInt(), 4);
    EXPECT_EQ(titles(model), QStringList({"0.mp4", "1.mp4", "5.mp4", "6.mp4", "9.mp4"}));

    QList<int> all;
    for (int row = 0; row < model.count(); ++row) {
        all.append(model.idAt(row));
    }
    removed.clear();
    EXPECT_EQ(model.remove(all), 5);
    EXPECT_EQ(removed.size(), 1);
    EXPECT_EQ(model.count(), 0);
}

TEST(PlaylistModelTest, AnchorMarksWhereTheRemovedCurrentEntryWas) {
    PlaylistModel model;
    model.append(files(4));
    model.setCurrentRow(1);
    model.remove({model.currentId()});
    EXPECT_EQ(model.anchorRow(), 1);
    model.insert(0, files(1, 50));
    EXPECT_EQ(model.anchorRow(), 2);
    model.remove({model.idAt(0)});
    EXPECT_EQ(model.anchorRow(), 1);
    model.setCurrentRow(0);
    EXPECT_EQ(model.anchorRow(), -1);
}

TEST(PlaylistModelTest, ClearResetsEverything) {
    PlaylistModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append(files(3));
    model.setCurrentRow(2);
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    QSignalSpy current(&model, &PlaylistModel::currentChanged);
    model.clear();
    EXPECT_EQ(reset.size(), 1);
    EXPECT_EQ(current.size(), 1);
    EXPECT_EQ(model.count(), 0);
    EXPECT_EQ(model.currentRow(), -1);
    EXPECT_EQ(model.anchorRow(), -1);
    model.clear();
    EXPECT_EQ(reset.size(), 1);
}

TEST(PlaylistModelTest, ListsComeBackInPlaylistOrder) {
    PlaylistModel model;
    model.append({item("file:///b.mp4", "B"), item("https://example.org/a.m3u8", "A")});
    const QVariantList all = model.toVariantList();
    ASSERT_EQ(all.size(), 2);
    EXPECT_EQ(all[0].toMap().value("title").toString(), "B");
    EXPECT_EQ(all[1].toMap().value("path").toString(), "https://example.org/a.m3u8");
    EXPECT_EQ(model.locations(), QList<QUrl>({QUrl("file:///b.mp4"), QUrl("https://example.org/a.m3u8")}));
    EXPECT_TRUE(model.entryAt(5).isEmpty());
}

TEST(PlaylistModelTest, LoadingAnEntryRecordsWhatThePlayerLearned) {
    PlaylistModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append({item("https://example.org/film.m3u8", "Film"), item("https://example.org/news.m3u8", "News"), item("file:///videos/a.mp4")});
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    model.setLoaded(model.idAt(0), 600000, false);
    ASSERT_EQ(changed.count(), 1);
    EXPECT_EQ(changed[0][0].toModelIndex().row(), 0);
    EXPECT_EQ(changed[0][1].toModelIndex().row(), 0);
    EXPECT_EQ(model.entry(0)->availability, PlaylistModel::Playable);
    EXPECT_EQ(model.entry(0)->streamState, PlaylistModel::StreamOnDemand);
    EXPECT_EQ(model.entry(0)->durationMs, 600000);
    EXPECT_EQ(model.entryAt(0).value("durationSec").toInt(), 600);

    model.setLoaded(model.idAt(1), 0, true);
    ASSERT_EQ(changed.count(), 2);
    EXPECT_EQ(changed[1][0].toModelIndex().row(), 1);
    EXPECT_EQ(model.entry(1)->availability, PlaylistModel::Playable);
    EXPECT_EQ(model.entry(1)->streamState, PlaylistModel::StreamLive);
    EXPECT_EQ(model.entry(1)->durationMs, -1);
    EXPECT_EQ(model.data(model.index(1), PlaylistModel::StreamStateRole).toInt(), PlaylistModel::StreamLive);

    model.setLoaded(model.idAt(2), 30000, false);
    ASSERT_EQ(changed.count(), 3);
    EXPECT_EQ(model.entry(2)->availability, PlaylistModel::Playable);
    EXPECT_EQ(model.entry(2)->streamState, PlaylistModel::StreamUnknown);
    EXPECT_EQ(model.entry(2)->durationMs, 30000);

    model.setLoaded(model.idAt(2), 30000, false);
    model.setLoaded(999, 1000, false);
    EXPECT_EQ(changed.count(), 3);
}

TEST(PlaylistModelTest, LoadedDurationDoesNotReplaceAKnownOneWithNothing) {
    PlaylistModel model;
    model.append({QVariantMap{{"path", "https://example.org/film.m3u8"}, {"durationSec", 600}}});
    model.setLoaded(model.idAt(0), 0, false);
    EXPECT_EQ(model.entry(0)->durationMs, 600000);
    EXPECT_EQ(model.entry(0)->streamState, PlaylistModel::StreamOnDemand);
}

TEST(PlaylistModelTest, FailuresMarkAnEntryUnavailableUntilItLoads) {
    PlaylistModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append(files(3));
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    model.setUnavailable(model.idAt(1), "Could not open the file");
    ASSERT_EQ(changed.count(), 1);
    EXPECT_EQ(changed[0][0].toModelIndex().row(), 1);
    EXPECT_EQ(changed[0][1].toModelIndex().row(), 1);
    EXPECT_EQ(model.entry(1)->availability, PlaylistModel::Unavailable);
    EXPECT_FALSE(model.data(model.index(1), PlaylistModel::AvailableRole).toBool());
    EXPECT_EQ(model.data(model.index(1), PlaylistModel::ReasonRole).toString(), "Could not open the file");
    EXPECT_TRUE(model.data(model.index(0), PlaylistModel::AvailableRole).toBool());

    model.setLoaded(model.idAt(1), 30000, false);
    ASSERT_EQ(changed.count(), 2);
    EXPECT_EQ(model.entry(1)->availability, PlaylistModel::Playable);
    EXPECT_TRUE(model.data(model.index(1), PlaylistModel::AvailableRole).toBool());
    EXPECT_TRUE(model.data(model.index(1), PlaylistModel::ReasonRole).toString().isEmpty());

    model.setUnavailable(model.idAt(1), "Network error");
    EXPECT_EQ(model.entry(1)->availability, PlaylistModel::Unavailable);
    EXPECT_EQ(model.entryAt(1).value("reason").toString(), "Network error");
    model.setUnavailable(999, "Nothing");
    EXPECT_EQ(changed.count(), 3);
}
