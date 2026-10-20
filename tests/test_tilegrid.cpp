#include <gtest/gtest.h>
#include "../tilegrid.h"

#include <QSignalSpy>

namespace {

QList<int> list(const QVariantMap &result, const char *key) { return result.value(QString::fromLatin1(key)).value<QList<int>>(); }

bool flag(const QVariantMap &result, const char *key) { return result.value(QString::fromLatin1(key)).toBool(); }

const QList<int> none;

} // namespace

TEST(TileGridTest, StartsEmptyWithSlotZeroSelected) {
    TileGrid grid;
    EXPECT_EQ(grid.count(), 0);
    EXPECT_FALSE(grid.splitView());
    EXPECT_EQ(grid.selectedSlot(), 0);
    EXPECT_FALSE(grid.focused());
    EXPECT_EQ(grid.focusedSlot(), -1);
    EXPECT_EQ(grid.entryIds(), QList<int>({-1, -1, -1, -1}));
}

TEST(TileGridTest, OpenPutsTheFirstVideoInSlotZero) {
    TileGrid grid;
    const QVariantMap result = grid.open(7);
    EXPECT_EQ(list(result, "load"), QList<int>({0}));
    EXPECT_EQ(grid.count(), 1);
    EXPECT_FALSE(grid.splitView());
    EXPECT_EQ(grid.selectedSlot(), 0);
    EXPECT_EQ(grid.positionOf(0), 1);
}

TEST(TileGridTest, OpenInTheSingleViewReplacesTheOnlyVideo) {
    TileGrid grid;
    grid.open(7);
    EXPECT_EQ(list(grid.open(8), "load"), QList<int>({0}));
    EXPECT_EQ(grid.count(), 1);
    EXPECT_EQ(grid.entryIds().at(0), 8);
}

TEST(TileGridTest, OpeningThePlayingEntryAgainLoadsNothing) {
    TileGrid grid;
    grid.open(7);
    EXPECT_TRUE(list(grid.open(7), "load").isEmpty());
    EXPECT_EQ(grid.entryIds().at(0), 7);
}

TEST(TileGridTest, OpenInASplitViewReplacesTheSelectedVideo) {
    TileGrid grid;
    grid.start({1, 2, 3});
    grid.select(1);
    EXPECT_EQ(list(grid.open(9), "load"), QList<int>({1}));
    EXPECT_EQ(grid.entryIds(), QList<int>({1, 9, 3, -1}));
    EXPECT_EQ(grid.positionOf(1), 2);
    EXPECT_EQ(grid.selectedSlot(), 1);
}

TEST(TileGridTest, OpenOfAnEntryInAnotherTileSelectsThatTile) {
    TileGrid grid;
    grid.start({1, 2, 3});
    EXPECT_TRUE(list(grid.open(3), "load").isEmpty());
    EXPECT_EQ(grid.selectedSlot(), 2);
    EXPECT_EQ(grid.entryIds(), QList<int>({1, 2, 3, -1}));
}

TEST(TileGridTest, StartNeedsTwoToFourEntries) {
    TileGrid grid;
    EXPECT_TRUE(flag(grid.start({1}), "refused"));
    EXPECT_TRUE(flag(grid.start({1, 2, 3, 4, 5}), "refused"));
    EXPECT_EQ(grid.count(), 0);
    EXPECT_TRUE(flag(grid.start({1, 1}), "refused"));
}

TEST(TileGridTest, StartFillsSlotsInOrderAndSelectsTheFirst) {
    TileGrid grid;
    const QVariantMap result = grid.start({5, 6, 7, 8});
    EXPECT_FALSE(flag(result, "refused"));
    EXPECT_EQ(list(result, "load"), QList<int>({0, 1, 2, 3}));
    EXPECT_EQ(grid.count(), 4);
    EXPECT_TRUE(grid.splitView());
    EXPECT_EQ(grid.selectedSlot(), 0);
    for (int slot = 0; slot < 4; ++slot) {
        EXPECT_EQ(grid.positionOf(slot), slot + 1);
    }
}

TEST(TileGridTest, StartIsRefusedWhenSomethingPlays) {
    TileGrid grid;
    grid.open(1);
    EXPECT_TRUE(flag(grid.start({2, 3}), "refused"));
}

TEST(TileGridTest, AddKeepsThePlayingVideoFirstAndSelected) {
    TileGrid grid;
    grid.open(1);
    const QVariantMap result = grid.add({2, 3});
    EXPECT_EQ(list(result, "load"), QList<int>({1, 2}));
    EXPECT_EQ(grid.count(), 3);
    EXPECT_EQ(grid.selectedSlot(), 0);
    EXPECT_EQ(grid.positionOf(0), 1);
    EXPECT_EQ(grid.positionOf(1), 2);
    EXPECT_EQ(grid.positionOf(2), 3);
}

TEST(TileGridTest, AddLeavesOutEntriesAlreadyInTheGrid) {
    TileGrid grid;
    grid.start({1, 2});
    grid.select(1);
    EXPECT_EQ(list(grid.add({2, 3, 1}), "load"), QList<int>({2}));
    EXPECT_EQ(grid.entryIds(), QList<int>({1, 2, 3, -1}));
    EXPECT_EQ(grid.selectedSlot(), 1);
}

TEST(TileGridTest, AddIsRefusedAsAWholeBeyondFour) {
    TileGrid grid;
    grid.start({1, 2, 3});
    EXPECT_TRUE(flag(grid.add({4, 5}), "refused"));
    EXPECT_EQ(grid.count(), 3);
    EXPECT_FALSE(flag(grid.add({4}), "refused"));
    EXPECT_EQ(grid.count(), 4);
}

TEST(TileGridTest, AddWithNothingNewIsRefused) {
    TileGrid grid;
    grid.start({1, 2});
    EXPECT_TRUE(flag(grid.add({1, 2}), "refused"));
}

TEST(TileGridTest, AddWithNothingPlayingNeedsTwoEntries) {
    TileGrid grid;
    EXPECT_TRUE(flag(grid.add({1}), "refused"));
    EXPECT_FALSE(flag(grid.add({1, 2}), "refused"));
    EXPECT_EQ(grid.count(), 2);
}

TEST(TileGridTest, CanAddFollowsTheCountRules) {
    TileGrid grid;
    EXPECT_FALSE(grid.canAdd({1}));
    EXPECT_TRUE(grid.canAdd({1, 2}));
    EXPECT_FALSE(grid.canAdd({1, 2, 3, 4, 5}));
    grid.open(1);
    EXPECT_FALSE(grid.canAdd({1}));
    EXPECT_TRUE(grid.canAdd({1, 2}));
    EXPECT_EQ(grid.resultingCount({1, 2}), 2);
    EXPECT_TRUE(grid.canAdd({2, 3, 4}));
    EXPECT_FALSE(grid.canAdd({2, 3, 4, 5}));
    EXPECT_EQ(grid.resultingCount({2, 3, 4, 5}), 5);
}

TEST(TileGridTest, ReplaceKeepsPositionAndSelection) {
    TileGrid grid;
    grid.start({1, 2, 3});
    EXPECT_EQ(list(grid.replace(2, 9), "load"), QList<int>({2}));
    EXPECT_EQ(grid.entryIds(), QList<int>({1, 2, 9, -1}));
    EXPECT_EQ(grid.positionOf(2), 3);
    EXPECT_EQ(grid.selectedSlot(), 0);
}

TEST(TileGridTest, ReplaceWithAnEntryInAnotherSlotSelectsIt) {
    TileGrid grid;
    grid.start({1, 2, 3});
    EXPECT_TRUE(list(grid.replace(0, 3), "load").isEmpty());
    EXPECT_EQ(grid.entryIds(), QList<int>({1, 2, 3, -1}));
    EXPECT_EQ(grid.selectedSlot(), 2);
}

TEST(TileGridTest, ReplaceOfAnEmptySlotIsRefused) {
    TileGrid grid;
    grid.start({1, 2});
    EXPECT_TRUE(flag(grid.replace(3, 9), "refused"));
    EXPECT_EQ(grid.count(), 2);
}

TEST(TileGridTest, CloseKeepsTheOthersInOrderAndSelectsTheFirst) {
    TileGrid grid;
    grid.start({1, 2, 3, 4});
    grid.select(1);
    const QVariantMap result = grid.close(1);
    EXPECT_EQ(list(result, "closed"), QList<int>({1}));
    EXPECT_EQ(grid.count(), 3);
    EXPECT_EQ(grid.entryIds(), QList<int>({1, -1, 3, 4}));
    EXPECT_EQ(grid.positionOf(0), 1);
    EXPECT_EQ(grid.positionOf(2), 2);
    EXPECT_EQ(grid.positionOf(3), 3);
    EXPECT_EQ(grid.positionOf(1), 0);
    EXPECT_EQ(grid.selectedSlot(), 0);
}

TEST(TileGridTest, ClosingDownToOneLeavesTheSingleView) {
    TileGrid grid;
    grid.start({1, 2});
    grid.close(0);
    EXPECT_EQ(grid.count(), 1);
    EXPECT_FALSE(grid.splitView());
    EXPECT_EQ(grid.selectedSlot(), 1);
    EXPECT_EQ(grid.cellFor(1, 800, 600), QRectF(0, 0, 800, 600));
}

TEST(TileGridTest, ClosingTheLastVideoKeepsItsSlotSelected) {
    TileGrid grid;
    grid.start({1, 2});
    grid.close(0);
    grid.close(1);
    EXPECT_EQ(grid.count(), 0);
    EXPECT_EQ(grid.selectedSlot(), 1);
    EXPECT_EQ(list(grid.open(5), "load"), QList<int>({0}));
    EXPECT_EQ(grid.selectedSlot(), 0);
}

TEST(TileGridTest, AddingAfterACloseUsesTheFreeSlotAtTheEnd) {
    TileGrid grid;
    grid.start({1, 2, 3});
    grid.close(0);
    EXPECT_EQ(list(grid.add({9}), "load"), QList<int>({0}));
    EXPECT_EQ(grid.positionOf(1), 1);
    EXPECT_EQ(grid.positionOf(2), 2);
    EXPECT_EQ(grid.positionOf(0), 3);
}

TEST(TileGridTest, EndKeepsOnlyTheSelectedVideo) {
    TileGrid grid;
    grid.start({1, 2, 3});
    grid.select(2);
    const QVariantMap result = grid.end();
    EXPECT_EQ(list(result, "closed"), QList<int>({0, 1}));
    EXPECT_EQ(grid.count(), 1);
    EXPECT_EQ(grid.entryIds(), QList<int>({-1, -1, 3, -1}));
    EXPECT_EQ(grid.selectedSlot(), 2);
    EXPECT_EQ(grid.positionOf(2), 1);
}

TEST(TileGridTest, AnEntryIsInAtMostOneSlot) {
    TileGrid grid;
    grid.start({1, 2});
    grid.add({1, 2, 3});
    grid.replace(0, 2);
    grid.open(3);
    QList<int> ids = grid.entryIds();
    ids.removeAll(-1);
    EXPECT_EQ(QSet<int>(ids.begin(), ids.end()).size(), ids.size());
}

TEST(TileGridTest, EntryRemovedClosesItsTile) {
    TileGrid grid;
    grid.start({1, 2, 3});
    EXPECT_EQ(list(grid.entryRemoved(2), "closed"), QList<int>({1}));
    EXPECT_EQ(grid.count(), 2);
    EXPECT_TRUE(list(grid.entryRemoved(42), "closed").isEmpty());
}

TEST(TileGridTest, ClearEmptiesEverything) {
    TileGrid grid;
    grid.start({1, 2, 3});
    grid.clear();
    EXPECT_EQ(grid.count(), 0);
    EXPECT_EQ(grid.entryIds(), QList<int>({-1, -1, -1, -1}));
}

TEST(TileGridTest, CellsForEachCount) {
    TileGrid one;
    one.open(1);
    EXPECT_EQ(one.cellFor(0, 800, 600), QRectF(0, 0, 800, 600));

    TileGrid two;
    two.start({1, 2});
    EXPECT_EQ(two.cellFor(0, 800, 600), QRectF(0, 0, 400, 600));
    EXPECT_EQ(two.cellFor(1, 800, 600), QRectF(400, 0, 400, 600));

    TileGrid three;
    three.start({1, 2, 3});
    EXPECT_EQ(three.cellFor(0, 800, 600), QRectF(0, 0, 400, 300));
    EXPECT_EQ(three.cellFor(1, 800, 600), QRectF(400, 0, 400, 300));
    EXPECT_EQ(three.cellFor(2, 800, 600), QRectF(0, 300, 400, 300));
    EXPECT_TRUE(three.cellFor(3, 800, 600).isEmpty());

    TileGrid four;
    four.start({1, 2, 3, 4});
    EXPECT_EQ(four.cellFor(3, 1920, 1080), QRectF(960, 540, 960, 540));
}

TEST(TileGridTest, WithNothingPlayingTheSelectedSlotFillsTheArea) {
    TileGrid grid;
    EXPECT_EQ(grid.cellFor(0, 800, 600), QRectF(0, 0, 800, 600));
    EXPECT_TRUE(grid.cellFor(1, 800, 600).isEmpty());
}

TEST(TileGridTest, AnEmptySlotHasNoCell) {
    TileGrid grid;
    grid.start({1, 2});
    EXPECT_TRUE(grid.cellFor(2, 800, 600).isEmpty());
    EXPECT_TRUE(grid.cellFor(-1, 800, 600).isEmpty());
    EXPECT_TRUE(grid.cellFor(4, 800, 600).isEmpty());
}

TEST(TileGridTest, SlotAtFindsTheTileUnderAPoint) {
    TileGrid grid;
    grid.start({1, 2, 3});
    EXPECT_EQ(grid.slotAt(10, 10, 800, 600), 0);
    EXPECT_EQ(grid.slotAt(790, 10, 800, 600), 1);
    EXPECT_EQ(grid.slotAt(10, 590, 800, 600), 2);
    EXPECT_EQ(grid.slotAt(790, 590, 800, 600), -1);
    EXPECT_EQ(grid.slotAt(900, 10, 800, 600), -1);
}

TEST(TileGridTest, SlotAtPositionFollowsTheOrder) {
    TileGrid grid;
    grid.start({1, 2, 3});
    grid.close(0);
    EXPECT_EQ(grid.slotAtPosition(1), 1);
    EXPECT_EQ(grid.slotAtPosition(2), 2);
    EXPECT_EQ(grid.slotAtPosition(3), -1);
    EXPECT_EQ(grid.slotAtPosition(0), -1);
}

TEST(TileGridTest, PressPositionSelectsInTheGrid) {
    TileGrid grid;
    grid.start({1, 2, 3});
    EXPECT_TRUE(flag(grid.pressPosition(3, none), "handled"));
    EXPECT_EQ(grid.selectedSlot(), 2);
}

TEST(TileGridTest, PressPositionWithNoVideoDoesNothing) {
    TileGrid grid;
    grid.start({1, 2, 3});
    const QVariantMap result = grid.pressPosition(4, none);
    EXPECT_TRUE(flag(result, "handled"));
    EXPECT_EQ(grid.selectedSlot(), 0);
}

TEST(TileGridTest, PressPositionOutsideASplitViewIsNotHandled) {
    TileGrid grid;
    grid.open(1);
    EXPECT_FALSE(flag(grid.pressPosition(1, none), "handled"));
}

TEST(TileGridTest, DoubleClickInTheGridFocusesAndPausesPlayingFiles) {
    TileGrid grid;
    grid.start({1, 2, 3, 4});
    const QVariantMap result = grid.doubleClick(1, false, {0, 1, 3});
    EXPECT_TRUE(flag(result, "handled"));
    EXPECT_FALSE(flag(result, "enterFullScreen"));
    EXPECT_EQ(list(result, "pause"), QList<int>({0, 3}));
    EXPECT_TRUE(grid.focused());
    EXPECT_EQ(grid.focusedSlot(), 1);
    EXPECT_EQ(grid.selectedSlot(), 1);
    EXPECT_EQ(grid.cellFor(1, 800, 600), QRectF(0, 0, 800, 600));
    EXPECT_TRUE(grid.cellFor(0, 800, 600).isEmpty());
    EXPECT_TRUE(grid.cellFor(2, 800, 600).isEmpty());
}

TEST(TileGridTest, SecondDoubleClickGoesFullScreen) {
    TileGrid grid;
    grid.start({1, 2});
    grid.doubleClick(0, false, {1});
    const QVariantMap result = grid.doubleClick(0, false, none);
    EXPECT_TRUE(flag(result, "enterFullScreen"));
    EXPECT_FALSE(flag(result, "leaveFullScreen"));
    EXPECT_TRUE(grid.focused());
}

TEST(TileGridTest, DoubleClickInFullScreenReturnsToTheGrid) {
    TileGrid grid;
    grid.start({1, 2, 3});
    grid.doubleClick(0, false, {1, 2});
    const QVariantMap result = grid.doubleClick(0, true, none);
    EXPECT_TRUE(flag(result, "leaveFullScreen"));
    EXPECT_EQ(list(result, "resume"), QList<int>({1, 2}));
    EXPECT_FALSE(grid.focused());
    EXPECT_EQ(grid.selectedSlot(), 0);
    EXPECT_EQ(grid.cellFor(2, 800, 600), QRectF(0, 300, 400, 300));
}

TEST(TileGridTest, DoubleClickFromAFullScreenGridFocusesWithoutLeaving) {
    TileGrid grid;
    grid.start({1, 2});
    const QVariantMap result = grid.doubleClick(1, true, none);
    EXPECT_TRUE(flag(result, "handled"));
    EXPECT_FALSE(flag(result, "leaveFullScreen"));
    EXPECT_TRUE(grid.focused());
}

TEST(TileGridTest, DoubleClickOutsideASplitViewIsNotHandled) {
    TileGrid grid;
    grid.open(1);
    EXPECT_FALSE(flag(grid.doubleClick(0, false, none), "handled"));
    EXPECT_FALSE(grid.focused());
}

TEST(TileGridTest, EscapeInFullScreenLeavesItAndStaysFocused) {
    TileGrid grid;
    grid.start({1, 2});
    grid.doubleClick(0, false, {1});
    const QVariantMap result = grid.escape(true);
    EXPECT_TRUE(flag(result, "handled"));
    EXPECT_TRUE(flag(result, "leaveFullScreen"));
    EXPECT_TRUE(list(result, "resume").isEmpty());
    EXPECT_TRUE(grid.focused());
}

TEST(TileGridTest, EscapeWhileFocusedReturnsToTheGrid) {
    TileGrid grid;
    grid.start({1, 2});
    grid.doubleClick(0, false, {1});
    const QVariantMap result = grid.escape(false);
    EXPECT_TRUE(flag(result, "handled"));
    EXPECT_FALSE(flag(result, "leaveFullScreen"));
    EXPECT_EQ(list(result, "resume"), QList<int>({1}));
    EXPECT_FALSE(grid.focused());
}

TEST(TileGridTest, EscapeInTheGridIsNotHandled) {
    TileGrid grid;
    grid.start({1, 2});
    EXPECT_FALSE(flag(grid.escape(true), "handled"));
    EXPECT_FALSE(flag(grid.escape(false), "handled"));
}

TEST(TileGridTest, PressPositionWhileFocusedMovesTheFocus) {
    TileGrid grid;
    grid.start({1, 2, 3, 4});
    grid.doubleClick(0, false, {1, 2, 3});
    const QVariantMap result = grid.pressPosition(3, {0});
    EXPECT_EQ(list(result, "pause"), QList<int>({0}));
    EXPECT_EQ(list(result, "resume"), QList<int>({2}));
    EXPECT_EQ(grid.focusedSlot(), 2);
    EXPECT_EQ(grid.selectedSlot(), 2);
    EXPECT_EQ(list(grid.escape(false), "resume"), QList<int>({0, 1, 3}));
}

TEST(TileGridTest, MovingTheFocusDoesNotPauseALiveOrPausedVideo) {
    TileGrid grid;
    grid.start({1, 2});
    grid.doubleClick(0, false, {1});
    const QVariantMap result = grid.pressPosition(2, none);
    EXPECT_TRUE(list(result, "pause").isEmpty());
    EXPECT_EQ(list(result, "resume"), QList<int>({1}));
    EXPECT_TRUE(list(grid.escape(false), "resume").isEmpty());
}

TEST(TileGridTest, AVideoTheUserPausedIsNotResumed) {
    TileGrid grid;
    grid.start({1, 2, 3});
    grid.doubleClick(0, false, {0, 2});
    EXPECT_EQ(list(grid.escape(false), "resume"), QList<int>({2}));
}

TEST(TileGridTest, CloseWhileFocusedReturnsToTheGridAndResumes) {
    TileGrid grid;
    grid.start({1, 2, 3});
    grid.doubleClick(0, false, {1, 2});
    const QVariantMap result = grid.close(0);
    EXPECT_EQ(list(result, "closed"), QList<int>({0}));
    EXPECT_EQ(list(result, "resume"), QList<int>({1, 2}));
    EXPECT_FALSE(grid.focused());
}

TEST(TileGridTest, AddWhileFocusedReturnsToTheGrid) {
    TileGrid grid;
    grid.start({1, 2});
    grid.doubleClick(1, false, {0});
    const QVariantMap result = grid.add({3});
    EXPECT_EQ(list(result, "resume"), QList<int>({0}));
    EXPECT_FALSE(grid.focused());
    EXPECT_EQ(grid.selectedSlot(), 1);
}

TEST(TileGridTest, EndWhileFocusedKeepsTheFocusedVideo) {
    TileGrid grid;
    grid.start({1, 2, 3});
    grid.doubleClick(2, false, {0, 1});
    const QVariantMap result = grid.end();
    EXPECT_EQ(list(result, "closed"), QList<int>({0, 1}));
    EXPECT_TRUE(list(result, "resume").isEmpty());
    EXPECT_FALSE(grid.focused());
    EXPECT_EQ(grid.selectedSlot(), 2);
}

TEST(TileGridTest, ChangesAreSignalled) {
    TileGrid grid;
    QSignalSpy spy(&grid, &TileGrid::changed);
    grid.open(1);
    grid.add({2});
    grid.select(1);
    grid.select(1);
    grid.close(0);
    EXPECT_EQ(spy.count(), 4);
}
