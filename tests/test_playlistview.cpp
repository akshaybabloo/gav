#include <gtest/gtest.h>
#include "../playlistview.h"

#include <QAbstractItemModelTester>
#include <QElapsedTimer>
#include <QSignalSpy>

#include <iostream>

namespace {

QVariantList files(int count, int first = 0) {
    QVariantList list;
    for (int i = 0; i < count; ++i) {
        list.append(QVariantMap{{"path", QStringLiteral("file:///videos/%1.mp4").arg(first + i)}});
    }
    return list;
}

QVariantMap entry(const QString &path, const QString &title, const QString &group = QString(), int durationSec = -1) {
    QVariantMap map{{"path", path}, {"title", title}, {"durationSec", durationSec}};
    if (!group.isEmpty()) {
        map.insert("group", group);
    }
    return map;
}

QVariantList mixed() {
    return {entry("file:///videos/delta.mp4", "delta", "Films", 300),  entry("https://example.org/news.m3u8", "Bravo News", "News"),
            entry("file:///music/alpha.mp3", "Alpha", QString(), 60), entry("https://example.org/film.m3u8", "charlie", "Films", 120),
            entry("file:///videos/echo.mp4", "Echo", "News", 60),     entry("file:///videos/foxtrot.mp4", "foxtrot")};
}

QVariantMap missing(const QString &name) { return {{"path", "file:///videos/" + name}, {"available", false}, {"reason", "File not found"}}; }

class PlaylistViewTest : public ::testing::Test {
protected:
    void SetUp() override { view.setSource(&model); }

    QString titleAt(int sourceRow) const { return model.entryAt(sourceRow).value("title").toString(); }

    QStringList shown() const {
        QStringList list;
        for (int row = 0; row < view.rowCount(); ++row) {
            const QModelIndex index = view.index(row);
            if (view.data(index, PlaylistView::IsHeaderRole).toBool()) {
                list.append(QStringLiteral("[%1:%2%3]")
                                .arg(view.data(index, PlaylistModel::GroupRole).toString())
                                .arg(view.data(index, PlaylistView::GroupCountRole).toInt())
                                .arg(view.data(index, PlaylistView::CollapsedRole).toBool() ? QStringLiteral(" collapsed") : QString()));
            } else {
                list.append(view.data(index, PlaylistModel::TitleRole).toString());
            }
        }
        return list;
    }

    QStringList sourceTitles() const {
        QStringList list;
        for (int row = 0; row < model.count(); ++row) {
            list.append(titleAt(row));
        }
        return list;
    }

    PlaylistModel model;
    PlaylistView view;
};

}

TEST_F(PlaylistViewTest, MirrorsTheSourceWhenNothingIsFiltered) {
    QAbstractItemModelTester tester(&view, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append(files(4));
    ASSERT_EQ(view.rowCount(), 4);
    for (int row = 0; row < 4; ++row) {
        EXPECT_EQ(view.sourceRowFor(row), row);
        EXPECT_EQ(view.viewRowFor(row), row);
        EXPECT_EQ(view.data(view.index(row), PlaylistModel::TitleRole), model.data(model.index(row), PlaylistModel::TitleRole));
        EXPECT_EQ(view.data(view.index(row), PlaylistView::SourceRowRole).toInt(), row);
        EXPECT_FALSE(view.data(view.index(row), PlaylistView::IsHeaderRole).toBool());
        EXPECT_FALSE(view.data(view.index(row), PlaylistView::SelectedRole).toBool());
    }
    EXPECT_EQ(view.sourceRowFor(-1), -1);
    EXPECT_EQ(view.sourceRowFor(4), -1);
    EXPECT_EQ(view.viewRowFor(-1), -1);
    EXPECT_TRUE(view.roleNames().values().contains("title"));
    EXPECT_TRUE(view.roleNames().values().contains("isHeader"));
}

TEST_F(PlaylistViewTest, ForwardsSourceChangesRowForRow) {
    QAbstractItemModelTester tester(&view, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy inserted(&view, &QAbstractItemModel::rowsInserted);
    QSignalSpy removed(&view, &QAbstractItemModel::rowsRemoved);
    QSignalSpy reset(&view, &QAbstractItemModel::modelReset);
    QSignalSpy visible(&view, &PlaylistView::visibleEntriesChanged);

    model.append(files(5));
    ASSERT_EQ(inserted.size(), 1);
    EXPECT_EQ(inserted[0][2].toInt(), 4);
    EXPECT_EQ(visible.size(), 1);

    model.remove({model.idAt(1), model.idAt(3)});
    EXPECT_EQ(removed.size(), 2);
    EXPECT_EQ(view.rowCount(), 3);
    EXPECT_EQ(view.visibleIds(), QList<int>({model.idAt(0), model.idAt(1), model.idAt(2)}));

    model.clear();
    EXPECT_EQ(reset.size(), 1);
    EXPECT_EQ(view.rowCount(), 0);
    EXPECT_TRUE(view.visibleIds().isEmpty());
}

TEST_F(PlaylistViewTest, NextAndPreviousWalkTheList) {
    model.append(files(3));
    model.setCurrentRow(0);
    EXPECT_EQ(view.currentViewRow(), 0);
    EXPECT_EQ(view.nextRow(false), 1);
    EXPECT_EQ(view.previousRow(), -1);

    model.setCurrentRow(2);
    EXPECT_EQ(view.nextRow(false), -1);
    EXPECT_EQ(view.nextRow(true), 0);
    EXPECT_EQ(view.previousRow(), 1);
}

TEST_F(PlaylistViewTest, SkipsUnavailableEntries) {
    model.append(files(1));
    model.append({missing("gone-1.mp4"), missing("gone-2.mp4")});
    model.append(files(1, 9));
    model.append({missing("gone-3.mp4")});

    model.setCurrentRow(0);
    EXPECT_EQ(view.nextRow(false), 3);
    model.setCurrentRow(3);
    EXPECT_EQ(view.previousRow(), 0);
    EXPECT_EQ(view.nextRow(false), -1);
    EXPECT_EQ(view.nextRow(true), 0);
}

TEST_F(PlaylistViewTest, WithoutACurrentEntryNextStartsAtTheTop) {
    model.append({missing("gone.mp4")});
    model.append(files(2));
    EXPECT_EQ(view.currentViewRow(), -1);
    EXPECT_EQ(view.nextRow(false), 1);
    EXPECT_EQ(view.previousRow(), -1);
}

TEST_F(PlaylistViewTest, AfterTheCurrentEntryIsRemovedNextContinuesFromItsPlace) {
    model.append(files(5));
    model.setCurrentRow(2);
    model.remove({model.currentId()});
    EXPECT_EQ(titleAt(view.nextRow(false)), "3.mp4");
    EXPECT_EQ(titleAt(view.previousRow()), "1.mp4");
}

TEST_F(PlaylistViewTest, ReportsWhetherNextAndPreviousExist) {
    QSignalSpy navigation(&view, &PlaylistView::navigationChanged);
    EXPECT_FALSE(view.canGoNext());
    model.append(files(2));
    EXPECT_TRUE(view.canGoNext());
    EXPECT_FALSE(view.canGoPrevious());
    model.setCurrentRow(1);
    EXPECT_FALSE(view.canGoNext());
    EXPECT_TRUE(view.canGoPrevious());
    EXPECT_GE(navigation.size(), 2);
    model.clear();
    EXPECT_FALSE(view.canGoPrevious());
}

TEST_F(PlaylistViewTest, CurrentViewRowFollowsTheSource) {
    model.append(files(3));
    QSignalSpy current(&view, &PlaylistView::currentViewRowChanged);
    model.setCurrentRow(1);
    EXPECT_EQ(view.currentViewRow(), 1);
    EXPECT_EQ(current.size(), 1);
    model.insert(0, files(1, 7));
    EXPECT_EQ(view.currentViewRow(), 2);
    EXPECT_EQ(current.size(), 2);
    EXPECT_TRUE(view.data(view.index(2), PlaylistModel::IsCurrentRole).toBool());
}

TEST_F(PlaylistViewTest, PlayableIdsLeaveOutUnavailableEntries) {
    model.append({files(1)[0], missing("gone.mp4"), files(1, 1)[0]});
    QSignalSpy playable(&view, &PlaylistView::playableEntriesChanged);
    EXPECT_EQ(view.playableIds(), QList<int>({model.idAt(0), model.idAt(2)}));
    EXPECT_EQ(view.visibleIds().size(), 3);

    model.setUnavailable(model.idAt(0), "Network error");
    EXPECT_EQ(playable.count(), 1);
    EXPECT_EQ(view.playableIds(), QList<int>({model.idAt(2)}));

    model.setLoaded(model.idAt(1), 30000, false);
    EXPECT_EQ(playable.count(), 2);
    EXPECT_EQ(view.playableIds(), QList<int>({model.idAt(1), model.idAt(2)}));

    model.setLoaded(model.idAt(2), 30000, false);
    EXPECT_EQ(view.playableIds().size(), 2);
}

TEST_F(PlaylistViewTest, SearchMatchesTitleAndGroupIgnoringCase) {
    QAbstractItemModelTester tester(&view, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append(mixed());
    QSignalSpy reset(&view, &QAbstractItemModel::modelReset);
    QSignalSpy matches(&view, &PlaylistView::matchCountChanged);
    EXPECT_EQ(view.matchCount(), 6);

    view.setSearchText("NEWS");
    EXPECT_EQ(shown(), QStringList({"Bravo News", "Echo"}));
    EXPECT_EQ(view.matchCount(), 2);
    EXPECT_EQ(reset.count(), 1);
    EXPECT_EQ(matches.count(), 1);
    EXPECT_EQ(view.sourceRowFor(1), 4);
    EXPECT_EQ(view.viewRowFor(4), 1);
    EXPECT_EQ(view.viewRowFor(0), -1);

    view.setSearchText("  ALP ");
    EXPECT_EQ(shown(), QStringList({"Alpha"}));
    view.setSearchText("films");
    EXPECT_EQ(shown(), QStringList({"delta", "charlie"}));
    view.setSearchText("no such thing");
    EXPECT_EQ(view.rowCount(), 0);
    EXPECT_EQ(view.matchCount(), 0);
    view.setSearchText("");
    EXPECT_EQ(view.rowCount(), 6);

    reset.clear();
    view.setSearchText("   ");
    EXPECT_EQ(reset.count(), 0);
    EXPECT_EQ(view.searchText(), "   ");
}

TEST_F(PlaylistViewTest, FiltersByLocalFilesAndStreams) {
    QAbstractItemModelTester tester(&view, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append(mixed());
    view.setFilter(PlaylistView::LocalFiles);
    EXPECT_EQ(shown(), QStringList({"delta", "Alpha", "Echo", "foxtrot"}));
    EXPECT_EQ(view.matchCount(), 4);
    view.setFilter(PlaylistView::Streams);
    EXPECT_EQ(shown(), QStringList({"Bravo News", "charlie"}));
    view.setSearchText("news");
    EXPECT_EQ(shown(), QStringList({"Bravo News"}));
    view.setFilter(PlaylistView::All);
    EXPECT_EQ(shown(), QStringList({"Bravo News", "Echo"}));
    EXPECT_EQ(QMetaEnum::fromType<PlaylistView::Filter>().keyCount(), 3);
}

TEST_F(PlaylistViewTest, SortsByTitleAndDurationAndBack) {
    QAbstractItemModelTester tester(&view, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append(mixed());
    view.setSortOrder(PlaylistView::Title);
    EXPECT_EQ(shown(), QStringList({"Alpha", "Bravo News", "charlie", "delta", "Echo", "foxtrot"}));
    EXPECT_EQ(view.sourceRowFor(0), 2);

    view.setSortOrder(PlaylistView::Duration);
    EXPECT_EQ(shown(), QStringList({"Alpha", "Echo", "charlie", "delta", "Bravo News", "foxtrot"}));

    view.setSortOrder(PlaylistView::PlaylistOrder);
    EXPECT_EQ(shown(), sourceTitles());
}

TEST_F(PlaylistViewTest, TitleSortPutsNumbersInCountingOrder) {
    model.append({entry("https://example.org/10", "Channel 10"), entry("https://example.org/2", "channel 2"), entry("https://example.org/1", "Channel 1")});
    view.setSortOrder(PlaylistView::Title);
    EXPECT_EQ(shown(), QStringList({"Channel 1", "channel 2", "Channel 10"}));
}

TEST_F(PlaylistViewTest, GroupsEntriesUnderHeaders) {
    QAbstractItemModelTester tester(&view, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy groups(&view, &PlaylistView::hasGroupsChanged);
    EXPECT_FALSE(view.hasGroups());
    model.append(mixed());
    EXPECT_TRUE(view.hasGroups());
    EXPECT_EQ(groups.count(), 1);

    view.setGrouped(true);
    EXPECT_EQ(shown(), QStringList({"[Films:2]", "delta", "charlie", "[News:2]", "Bravo News", "Echo", "[:2]", "Alpha", "foxtrot"}));
    EXPECT_EQ(view.sourceRowFor(0), -1);
    EXPECT_EQ(view.sourceRowFor(1), 0);
    EXPECT_EQ(view.viewRowFor(5), 8);
    EXPECT_EQ(view.matchCount(), 6);

    view.setSortOrder(PlaylistView::Title);
    EXPECT_EQ(shown(), QStringList({"[Films:2]", "charlie", "delta", "[News:2]", "Bravo News", "Echo", "[:2]", "Alpha", "foxtrot"}));

    view.setSortOrder(PlaylistView::PlaylistOrder);
    view.setSearchText("e");
    EXPECT_EQ(shown(), QStringList({"[Films:2]", "delta", "charlie", "[News:2]", "Bravo News", "Echo"}));
    view.setSearchText("echo");
    EXPECT_EQ(shown(), QStringList({"[News:1]", "Echo"}));
}

TEST_F(PlaylistViewTest, GroupingDoesNothingWithoutGroups) {
    model.append(files(3));
    view.setGrouped(true);
    EXPECT_TRUE(view.grouped());
    EXPECT_FALSE(view.hasGroups());
    EXPECT_EQ(view.rowCount(), 3);
    EXPECT_TRUE(view.canReorder());

    model.append({entry("https://example.org/news.m3u8", "News", "News")});
    EXPECT_TRUE(view.hasGroups());
    EXPECT_FALSE(view.canReorder());
    EXPECT_EQ(shown(), QStringList({"[News:1]", "News", "[:3]", "0.mp4", "1.mp4", "2.mp4"}));
}

TEST_F(PlaylistViewTest, CollapsingAGroupHidesItsRowsAndKeepsItsCount) {
    QAbstractItemModelTester tester(&view, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append(mixed());
    view.setGrouped(true);
    QSignalSpy removed(&view, &QAbstractItemModel::rowsRemoved);
    QSignalSpy inserted(&view, &QAbstractItemModel::rowsInserted);
    QSignalSpy reset(&view, &QAbstractItemModel::modelReset);
    QSignalSpy visible(&view, &PlaylistView::visibleEntriesChanged);

    view.toggleGroup("News");
    EXPECT_EQ(shown(), QStringList({"[Films:2]", "delta", "charlie", "[News:2 collapsed]", "[:2]", "Alpha", "foxtrot"}));
    ASSERT_EQ(removed.count(), 1);
    EXPECT_EQ(removed[0][1].toInt(), 4);
    EXPECT_EQ(removed[0][2].toInt(), 5);
    EXPECT_EQ(view.matchCount(), 6);
    EXPECT_EQ(view.viewRowFor(1), -1);
    EXPECT_EQ(view.visibleIds().size(), 4);
    EXPECT_EQ(visible.count(), 1);

    view.toggleGroup("");
    EXPECT_EQ(shown(), QStringList({"[Films:2]", "delta", "charlie", "[News:2 collapsed]", "[:2 collapsed]"}));

    view.setSearchText("bravo");
    EXPECT_EQ(shown(), QStringList({"[News:1 collapsed]"}));
    EXPECT_EQ(view.matchCount(), 1);
    view.setSearchText("");
    reset.clear();

    view.toggleGroup("News");
    EXPECT_EQ(shown(), QStringList({"[Films:2]", "delta", "charlie", "[News:2]", "Bravo News", "Echo", "[:2 collapsed]"}));
    ASSERT_EQ(inserted.count(), 1);
    EXPECT_EQ(inserted[0][1].toInt(), 4);
    EXPECT_EQ(inserted[0][2].toInt(), 5);
    EXPECT_EQ(reset.count(), 0);

    view.setGrouped(false);
    EXPECT_EQ(view.rowCount(), 6);
    view.toggleGroup("Films");
    EXPECT_EQ(view.rowCount(), 6);
    view.setGrouped(true);
    EXPECT_EQ(shown(), QStringList({"[Films:2 collapsed]", "[News:2]", "Bravo News", "Echo", "[:2 collapsed]"}));
}

TEST_F(PlaylistViewTest, ReorderingIsOnlyAllowedInThePlainView) {
    model.append(mixed());
    QSignalSpy changed(&view, &PlaylistView::canReorderChanged);
    EXPECT_TRUE(view.canReorder());
    view.setSearchText("a");
    EXPECT_FALSE(view.canReorder());
    view.setSearchText("");
    EXPECT_TRUE(view.canReorder());
    view.setFilter(PlaylistView::Streams);
    EXPECT_FALSE(view.canReorder());
    view.setFilter(PlaylistView::All);
    view.setSortOrder(PlaylistView::Duration);
    EXPECT_FALSE(view.canReorder());
    view.setSortOrder(PlaylistView::PlaylistOrder);
    view.setGrouped(true);
    EXPECT_FALSE(view.canReorder());
    view.setGrouped(false);
    EXPECT_TRUE(view.canReorder());
    EXPECT_EQ(changed.count(), 8);
}

TEST_F(PlaylistViewTest, NextAndPreviousFollowWhatIsShown) {
    model.append(mixed());
    view.setSortOrder(PlaylistView::Title);
    model.setCurrentRow(2);
    EXPECT_EQ(titleAt(view.nextRow(false)), "Bravo News");
    EXPECT_EQ(view.previousRow(), -1);
    model.setCurrentRow(5);
    EXPECT_EQ(view.nextRow(false), -1);
    EXPECT_EQ(titleAt(view.nextRow(true)), "Alpha");
    EXPECT_EQ(titleAt(view.previousRow()), "Echo");

    view.setSortOrder(PlaylistView::PlaylistOrder);
    view.setGrouped(true);
    model.setCurrentRow(3);
    EXPECT_EQ(titleAt(view.nextRow(false)), "Bravo News");
    EXPECT_EQ(titleAt(view.previousRow()), "delta");
    view.toggleGroup("News");
    EXPECT_EQ(titleAt(view.nextRow(false)), "Alpha");
    model.setCurrentRow(2);
    EXPECT_EQ(titleAt(view.previousRow()), "charlie");
    EXPECT_EQ(view.playableIds().size(), 4);

    view.setGrouped(false);
    view.setSearchText("e");
    model.setCurrentRow(2);
    EXPECT_EQ(view.currentViewRow(), -1);
    EXPECT_EQ(titleAt(view.nextRow(false)), "delta");
    EXPECT_EQ(view.previousRow(), -1);
    EXPECT_TRUE(view.canGoNext());
    EXPECT_FALSE(view.canGoPrevious());
    model.setCurrentRow(0);
    EXPECT_EQ(titleAt(view.nextRow(false)), "Bravo News");
    model.setCurrentRow(4);
    EXPECT_EQ(view.nextRow(false), -1);
}

TEST_F(PlaylistViewTest, ClearSearchAndFilterResetsBoth) {
    model.append(mixed());
    QSignalSpy reset(&view, &QAbstractItemModel::modelReset);
    QSignalSpy text(&view, &PlaylistView::searchTextChanged);
    QSignalSpy filter(&view, &PlaylistView::filterChanged);
    view.clearSearchAndFilter();
    EXPECT_EQ(reset.count(), 0);

    view.setSearchText("news");
    view.setFilter(PlaylistView::Streams);
    view.setSortOrder(PlaylistView::Title);
    reset.clear();
    text.clear();
    filter.clear();
    view.clearSearchAndFilter();
    EXPECT_TRUE(view.searchText().isEmpty());
    EXPECT_EQ(view.filter(), PlaylistView::All);
    EXPECT_EQ(view.sortOrder(), PlaylistView::Title);
    EXPECT_EQ(view.matchCount(), 6);
    EXPECT_EQ(reset.count(), 1);
    EXPECT_EQ(text.count(), 1);
    EXPECT_EQ(filter.count(), 1);
}

TEST_F(PlaylistViewTest, NeverChangesTheSourceOrder) {
    model.append(mixed());
    const QStringList before = sourceTitles();
    const QVariantList saved = model.toVariantList();
    view.setSortOrder(PlaylistView::Title);
    view.setGrouped(true);
    view.setFilter(PlaylistView::LocalFiles);
    view.setSearchText("a");
    view.toggleGroup("Films");
    view.setSortOrder(PlaylistView::Duration);
    view.clearSearchAndFilter();
    EXPECT_EQ(sourceTitles(), before);
    EXPECT_EQ(model.toVariantList(), saved);
}

TEST_F(PlaylistViewTest, RemovingEntriesWhileNarrowedKeepsTheRestInPlace) {
    QAbstractItemModelTester tester(&view, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append(mixed());
    view.setGrouped(true);
    view.setSortOrder(PlaylistView::Title);
    QSignalSpy removed(&view, &QAbstractItemModel::rowsRemoved);
    QSignalSpy reset(&view, &QAbstractItemModel::modelReset);
    QSignalSpy changed(&view, &QAbstractItemModel::dataChanged);
    ASSERT_EQ(shown(), QStringList({"[Films:2]", "charlie", "delta", "[News:2]", "Bravo News", "Echo", "[:2]", "Alpha", "foxtrot"}));

    model.remove({model.idAt(3)});
    EXPECT_EQ(shown(), QStringList({"[Films:1]", "delta", "[News:2]", "Bravo News", "Echo", "[:2]", "Alpha", "foxtrot"}));
    ASSERT_EQ(removed.count(), 1);
    EXPECT_EQ(removed[0][1].toInt(), 1);
    EXPECT_EQ(removed[0][2].toInt(), 1);
    EXPECT_EQ(reset.count(), 0);
    ASSERT_EQ(changed.count(), 1);
    EXPECT_EQ(changed[0][0].toModelIndex().row(), 0);
    EXPECT_EQ(view.sourceRowFor(4), 3);
    EXPECT_EQ(view.viewRowFor(3), 4);

    removed.clear();
    model.remove({model.idAt(1), model.idAt(3)});
    EXPECT_EQ(shown(), QStringList({"[Films:1]", "delta", "[:2]", "Alpha", "foxtrot"}));
    ASSERT_EQ(removed.count(), 2);
    EXPECT_EQ(removed[0][1].toInt(), 4);
    EXPECT_EQ(removed[0][2].toInt(), 4);
    EXPECT_EQ(removed[1][1].toInt(), 2);
    EXPECT_EQ(removed[1][2].toInt(), 3);
    EXPECT_EQ(reset.count(), 0);

    view.toggleGroup("");
    removed.clear();
    model.remove({model.idAt(1)});
    EXPECT_EQ(shown(), QStringList({"[Films:1]", "delta", "[:1 collapsed]"}));
    EXPECT_EQ(removed.count(), 0);
    EXPECT_EQ(view.matchCount(), 2);

    model.remove({model.idAt(0)});
    EXPECT_FALSE(view.hasGroups());
    EXPECT_EQ(shown(), QStringList({"foxtrot"}));
}

TEST_F(PlaylistViewTest, RemovingWhileSearchingRemovesJustThoseRows) {
    QAbstractItemModelTester tester(&view, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append(mixed());
    view.setSearchText("e");
    QSignalSpy removed(&view, &QAbstractItemModel::rowsRemoved);
    QSignalSpy reset(&view, &QAbstractItemModel::modelReset);
    ASSERT_EQ(shown(), QStringList({"delta", "Bravo News", "charlie", "Echo"}));

    model.remove({model.idAt(2)});
    EXPECT_EQ(removed.count(), 0);
    EXPECT_EQ(shown(), QStringList({"delta", "Bravo News", "charlie", "Echo"}));
    EXPECT_EQ(view.sourceRowFor(3), 3);

    model.remove({model.idAt(1)});
    EXPECT_EQ(shown(), QStringList({"delta", "charlie", "Echo"}));
    ASSERT_EQ(removed.count(), 1);
    EXPECT_EQ(removed[0][1].toInt(), 1);
    EXPECT_EQ(reset.count(), 0);
}

TEST_F(PlaylistViewTest, ReportsAddedEntriesThatTheSearchHides) {
    QAbstractItemModelTester tester(&view, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy hidden(&view, &PlaylistView::addedEntriesHidden);
    model.append(mixed());
    EXPECT_EQ(hidden.count(), 0);

    view.setSearchText("news");
    model.append({entry("file:///videos/golf.mp4", "Golf"), entry("https://example.org/world.m3u8", "World News"), entry("file:///videos/hotel.mp4", "Hotel")});
    ASSERT_EQ(hidden.count(), 1);
    EXPECT_EQ(hidden[0][0].toInt(), 3);
    EXPECT_EQ(hidden[0][1].toInt(), 2);
    EXPECT_EQ(shown(), QStringList({"Bravo News", "Echo", "World News"}));

    view.setSearchText("");
    view.setGrouped(true);
    view.toggleGroup("News");
    model.append({entry("https://example.org/local.m3u8", "Local News", "News")});
    EXPECT_EQ(hidden.count(), 1);
}

TEST_F(PlaylistViewTest, RebuildTimeIsLogged) {
    for (const int count : {10000, 50000}) {
        PlaylistModel large;
        PlaylistView narrowed;
        narrowed.setSource(&large);
        QVariantList entries;
        entries.reserve(count);
        for (int i = 0; i < count; ++i) {
            const int scrambled = int((qint64(i) * 7919) % count);
            entries.append(entry(QStringLiteral("https://example.org/live/%1.m3u8").arg(i), QStringLiteral("Channel %1").arg(scrambled, 5, 10, QLatin1Char('0')),
                                 QStringLiteral("Group %1").arg(i % 40), i % 3 ? scrambled % 7200 : -1));
        }
        QElapsedTimer timer;
        timer.start();
        large.append(entries);
        const double appendMs = double(timer.nsecsElapsed()) / 1e6;

        const auto timed = [&timer](const auto &change) {
            timer.restart();
            change();
            return double(timer.nsecsElapsed()) / 1e6;
        };
        const double searchMs = timed([&] { narrowed.setSearchText("channel 0734"); });
        const int found = narrowed.matchCount();
        const double clearMs = timed([&] { narrowed.setSearchText(""); });
        const double titleMs = timed([&] { narrowed.setSortOrder(PlaylistView::Title); });
        const double durationMs = timed([&] { narrowed.setSortOrder(PlaylistView::Duration); });
        const double groupMs = timed([&] { narrowed.setGrouped(true); });
        const double collapseMs = timed([&] { narrowed.toggleGroup("Group 7"); });
        const double filterMs = timed([&] { narrowed.setFilter(PlaylistView::Streams); });

        std::cout << count << " entries: append " << appendMs << " ms, search " << searchMs << " ms (" << found << " found), clear search " << clearMs
                  << " ms, sort by title " << titleMs << " ms, sort by duration " << durationMs << " ms, group " << groupMs << " ms, collapse "
                  << collapseMs << " ms, filter " << filterMs << " ms" << std::endl;
        EXPECT_GT(found, 0);
        EXPECT_EQ(narrowed.matchCount(), count);
    }
}

TEST_F(PlaylistViewTest, SelectionFollowsTheUsualClickRules) {
    QAbstractItemModelTester tester(&view, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append(files(6));
    QSignalSpy changed(&view, &PlaylistView::selectionChanged);
    const auto selected = [this](int row) { return view.data(view.index(row), PlaylistView::SelectedRole).toBool(); };

    view.select(2, Qt::NoModifier);
    EXPECT_EQ(view.selectedIds(), QList<int>({model.idAt(2)}));
    EXPECT_TRUE(selected(2));
    EXPECT_EQ(view.selectionCount(), 1);
    EXPECT_EQ(changed.count(), 1);

    view.select(4, Qt::ControlModifier);
    view.select(0, Qt::ControlModifier);
    EXPECT_EQ(view.selectedIds(), QList<int>({model.idAt(0), model.idAt(2), model.idAt(4)}));
    view.select(2, Qt::ControlModifier);
    EXPECT_EQ(view.selectedIds(), QList<int>({model.idAt(0), model.idAt(4)}));
    EXPECT_FALSE(selected(2));

    view.select(1, Qt::NoModifier);
    view.select(4, Qt::ShiftModifier);
    EXPECT_EQ(view.selectedIds(), QList<int>({model.idAt(1), model.idAt(2), model.idAt(3), model.idAt(4)}));
    view.select(0, Qt::ShiftModifier);
    EXPECT_EQ(view.selectedIds(), QList<int>({model.idAt(0), model.idAt(1)}));

    view.select(5, Qt::ControlModifier);
    view.select(3, Qt::ShiftModifier | Qt::ControlModifier);
    EXPECT_EQ(view.selectedIds(), QList<int>({model.idAt(0), model.idAt(1), model.idAt(3), model.idAt(4), model.idAt(5)}));

    view.select(3, Qt::NoModifier);
    EXPECT_EQ(view.selectionCount(), 1);
    view.clearSelection();
    EXPECT_EQ(view.selectionCount(), 0);
    EXPECT_TRUE(view.selectedIds().isEmpty());

    view.select(2, Qt::ShiftModifier);
    EXPECT_EQ(view.selectedIds(), QList<int>({model.idAt(2)}));
    view.select(99, Qt::NoModifier);
    EXPECT_EQ(view.selectedIds(), QList<int>({model.idAt(2)}));
}

TEST_F(PlaylistViewTest, SelectionCoversOnlyWhatIsShown) {
    QAbstractItemModelTester tester(&view, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append(mixed());
    view.setGrouped(true);
    view.select(0, Qt::NoModifier);
    EXPECT_EQ(view.selectionCount(), 0);
    EXPECT_FALSE(view.data(view.index(0), PlaylistView::SelectedRole).toBool());

    view.selectAll();
    EXPECT_EQ(view.selectionCount(), 6);
    EXPECT_EQ(view.selectedIds(), view.visibleIds());
    EXPECT_FALSE(view.data(view.index(3), PlaylistView::SelectedRole).toBool());

    view.select(1, Qt::NoModifier);
    view.select(5, Qt::ShiftModifier);
    EXPECT_EQ(view.selectedIds(), QList<int>({model.idAt(0), model.idAt(3), model.idAt(1), model.idAt(4)}));

    view.setGrouped(false);
    view.setSearchText("news");
    EXPECT_EQ(view.selectionCount(), 2);
    EXPECT_EQ(view.selectedIds(), QList<int>({model.idAt(1), model.idAt(4)}));
    view.selectAll();
    EXPECT_EQ(view.selectionCount(), 2);
    view.setSearchText("");
    EXPECT_EQ(view.selectedIds(), QList<int>({model.idAt(1), model.idAt(4)}));

    view.setSortOrder(PlaylistView::Title);
    view.select(0, Qt::NoModifier);
    view.select(2, Qt::ShiftModifier);
    EXPECT_EQ(view.selectedIds(), QList<int>({model.idAt(2), model.idAt(1), model.idAt(3)}));
}

TEST_F(PlaylistViewTest, RemovedEntriesLeaveTheSelection) {
    model.append(files(5));
    view.select(1, Qt::NoModifier);
    view.select(3, Qt::ControlModifier);
    QSignalSpy changed(&view, &PlaylistView::selectionChanged);
    const int kept = model.idAt(3);

    model.remove({model.idAt(1)});
    EXPECT_EQ(view.selectedIds(), QList<int>({kept}));
    EXPECT_EQ(view.selectionCount(), 1);
    EXPECT_GE(changed.count(), 1);

    model.undoRemove();
    EXPECT_EQ(view.selectedIds(), QList<int>({kept}));
    model.clear();
    EXPECT_EQ(view.selectionCount(), 0);
}

TEST_F(PlaylistViewTest, FollowsMovesInTheSource) {
    QAbstractItemModelTester tester(&view, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.append(mixed());
    QSignalSpy moved(&view, &QAbstractItemModel::rowsMoved);
    QSignalSpy reset(&view, &QAbstractItemModel::modelReset);
    view.select(4, Qt::NoModifier);
    const int selected = model.idAt(4);

    model.move({model.idAt(4), model.idAt(5)}, 1);
    EXPECT_EQ(shown(), QStringList({"delta", "Echo", "foxtrot", "Bravo News", "Alpha", "charlie"}));
    EXPECT_EQ(shown(), sourceTitles());
    EXPECT_GE(moved.count(), 1);
    EXPECT_EQ(reset.count(), 0);
    EXPECT_EQ(view.selectedIds(), QList<int>({selected}));
    EXPECT_EQ(view.viewRowFor(1), 1);

    view.setSortOrder(PlaylistView::Title);
    model.move({model.idAt(0)}, 6);
    EXPECT_EQ(shown(), QStringList({"Alpha", "Bravo News", "charlie", "delta", "Echo", "foxtrot"}));
    EXPECT_EQ(sourceTitles(), QStringList({"Echo", "foxtrot", "Bravo News", "Alpha", "charlie", "delta"}));
    view.setSearchText("o");
    EXPECT_EQ(shown(), QStringList({"Bravo News", "Echo", "foxtrot"}));
}
