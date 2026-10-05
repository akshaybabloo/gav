#include <gtest/gtest.h>
#include "../playlistview.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>

namespace {

QVariantList files(int count, int first = 0) {
    QVariantList list;
    for (int i = 0; i < count; ++i) {
        list.append(QVariantMap{{"path", QStringLiteral("file:///videos/%1.mp4").arg(first + i)}});
    }
    return list;
}

QVariantMap missing(const QString &name) { return {{"path", "file:///videos/" + name}, {"available", false}, {"reason", "File not found"}}; }

class PlaylistViewTest : public ::testing::Test {
protected:
    void SetUp() override { view.setSource(&model); }

    QString titleAt(int sourceRow) const { return model.entryAt(sourceRow).value("title").toString(); }

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
