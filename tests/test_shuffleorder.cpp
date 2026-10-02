#include <gtest/gtest.h>
#include "../shuffleorder.h"

#include <QSet>

#include <algorithm>

namespace {

QList<int> drain(ShuffleOrder &order) {
    QList<int> played;
    for (int index = order.next(false); index >= 0; index = order.next(false)) {
        played.append(index);
    }
    return played;
}

}

TEST(ShuffleOrder, EveryItemPlaysOncePerCycle) {
    for (const int size : {1, 2, 50}) {
        ShuffleOrder order;
        order.setSeed(42);
        order.reset(size, 0);
        QList<int> played{0};
        played.append(drain(order));
        EXPECT_EQ(played.size(), size) << size;
        EXPECT_EQ(QSet<int>(played.begin(), played.end()).size(), size) << size;
    }
}

TEST(ShuffleOrder, KeepsCurrentItemWhenEnabledMidPlaylist) {
    ShuffleOrder order;
    order.reset(10, 4);
    EXPECT_EQ(order.current(), 4);
    EXPECT_EQ(order.remaining(), 9);
    EXPECT_FALSE(drain(order).contains(4));
}

TEST(ShuffleOrder, NoNewCycleWithoutRepeat) {
    ShuffleOrder order;
    order.reset(3, 0);
    drain(order);
    EXPECT_EQ(order.next(false), -1);
}

TEST(ShuffleOrder, RepeatStartsNewCycleWithoutReplayingCurrent) {
    ShuffleOrder order;
    order.reset(3, 0);
    drain(order);
    const int last = order.current();
    const int first = order.next(true);
    EXPECT_GE(first, 0);
    EXPECT_NE(first, last);
    EXPECT_EQ(order.remaining(), 1);
}

TEST(ShuffleOrder, PreviousWalksBackAndNextReturnsForward) {
    ShuffleOrder order;
    order.reset(5, 0);
    const int second = order.next(false);
    const int third = order.next(false);
    EXPECT_EQ(order.previous(), second);
    EXPECT_EQ(order.next(false), third);
    EXPECT_EQ(order.previous(), second);
    EXPECT_TRUE(order.canGoBack());
    EXPECT_EQ(order.previous(), 0);
    EXPECT_FALSE(order.canGoBack());
    EXPECT_EQ(order.previous(), -1);
}

TEST(ShuffleOrder, RemovalRemapsRemainingIndices) {
    ShuffleOrder order;
    order.reset(5, 0);
    order.itemRemoved(2);
    QList<int> played{0};
    played.append(drain(order));
    std::sort(played.begin(), played.end());
    EXPECT_EQ(played, QList<int>({0, 1, 2, 3}));
}

TEST(ShuffleOrder, RemovingCurrentItemKeepsRemainingPlayable) {
    ShuffleOrder order;
    order.reset(4, 1);
    order.itemRemoved(1);
    QList<int> played = drain(order);
    std::sort(played.begin(), played.end());
    EXPECT_EQ(played, QList<int>({0, 1, 2}));
}

TEST(ShuffleOrder, MoveRemapsIndices) {
    ShuffleOrder order;
    order.reset(4, 0);
    order.itemMoved(0, 3);
    EXPECT_EQ(order.current(), 3);
    QList<int> played = drain(order);
    std::sort(played.begin(), played.end());
    EXPECT_EQ(played, QList<int>({0, 1, 2}));
}

TEST(ShuffleOrder, InsertedItemsGetPlayed) {
    ShuffleOrder order;
    order.reset(3, 0);
    order.next(false);
    order.itemInserted(3);
    order.itemInserted(0);
    EXPECT_EQ(order.current() >= 0, true);
    QList<int> all = drain(order);
    EXPECT_TRUE(all.contains(0));
    EXPECT_TRUE(all.contains(4));
}

TEST(ShuffleOrder, ManualSelectionIsNotReplayed) {
    ShuffleOrder order;
    order.reset(6, 0);
    order.setCurrent(3);
    EXPECT_EQ(order.current(), 3);
    EXPECT_FALSE(drain(order).contains(3));
}
