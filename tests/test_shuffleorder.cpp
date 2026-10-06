#include <gtest/gtest.h>
#include "../shuffleorder.h"

#include <QSet>

#include <algorithm>

namespace {

QList<int> ids(int count, int first = 100) {
    QList<int> list;
    for (int i = 0; i < count; ++i) {
        list.append(first + i);
    }
    return list;
}

QList<int> drain(ShuffleOrder &order) {
    QList<int> played;
    for (int id = order.next(false); id >= 0; id = order.next(false)) {
        played.append(id);
    }
    return played;
}

QList<int> sorted(QList<int> list) {
    std::sort(list.begin(), list.end());
    return list;
}

}

TEST(ShuffleOrder, EveryItemPlaysOncePerCycle) {
    for (const int size : {1, 2, 50}) {
        ShuffleOrder order;
        order.setSeed(42);
        order.reset(ids(size), 100);
        QList<int> played{100};
        played.append(drain(order));
        EXPECT_EQ(played.size(), size) << size;
        EXPECT_EQ(QSet<int>(played.begin(), played.end()).size(), size) << size;
    }
}

TEST(ShuffleOrder, KeepsCurrentItemWhenEnabledMidPlaylist) {
    ShuffleOrder order;
    order.reset(ids(10), 104);
    EXPECT_EQ(order.current(), 104);
    EXPECT_EQ(order.remaining(), 9);
    EXPECT_FALSE(drain(order).contains(104));
}

TEST(ShuffleOrder, UnknownCurrentIdStartsWithoutHistory) {
    ShuffleOrder order;
    order.reset(ids(3), 7);
    EXPECT_EQ(order.current(), -1);
    EXPECT_EQ(order.remaining(), 3);
}

TEST(ShuffleOrder, NoNewCycleWithoutRepeat) {
    ShuffleOrder order;
    order.reset(ids(3), 100);
    drain(order);
    EXPECT_EQ(order.next(false), -1);
}

TEST(ShuffleOrder, RepeatStartsNewCycleWithoutReplayingCurrent) {
    ShuffleOrder order;
    order.reset(ids(3), 100);
    drain(order);
    const int last = order.current();
    const int first = order.next(true);
    EXPECT_GE(first, 0);
    EXPECT_NE(first, last);
    EXPECT_EQ(order.remaining(), 1);
}

TEST(ShuffleOrder, PreviousWalksBackAndNextReturnsForward) {
    ShuffleOrder order;
    order.reset(ids(5), 100);
    const int second = order.next(false);
    const int third = order.next(false);
    EXPECT_EQ(order.previous(), second);
    EXPECT_EQ(order.next(false), third);
    EXPECT_EQ(order.previous(), second);
    EXPECT_TRUE(order.canGoBack());
    EXPECT_EQ(order.previous(), 100);
    EXPECT_FALSE(order.canGoBack());
    EXPECT_EQ(order.previous(), -1);
}

TEST(ShuffleOrder, RemovedCandidatesAreNeverPlayed) {
    ShuffleOrder order;
    order.reset(ids(5), 100);
    order.setCandidates({100, 101, 103, 104});
    QList<int> played{100};
    played.append(drain(order));
    EXPECT_EQ(sorted(played), QList<int>({100, 101, 103, 104}));
}

TEST(ShuffleOrder, RemovingCurrentItemKeepsRemainingPlayable) {
    ShuffleOrder order;
    order.reset(ids(4), 101);
    order.setCandidates({100, 102, 103});
    EXPECT_EQ(order.current(), -1);
    EXPECT_EQ(sorted(drain(order)), QList<int>({100, 102, 103}));
}

TEST(ShuffleOrder, ReorderingCandidatesKeepsTheRound) {
    ShuffleOrder order;
    order.reset(ids(4), 100);
    const int second = order.next(false);
    order.setCandidates({103, 102, 101, 100});
    EXPECT_EQ(order.current(), second);
    QList<int> played{100, second};
    played.append(drain(order));
    EXPECT_EQ(sorted(played), ids(4));
}

TEST(ShuffleOrder, AddedCandidatesJoinTheCurrentRound) {
    ShuffleOrder order;
    order.reset(ids(3), 100);
    order.next(false);
    order.setCandidates({99, 100, 101, 102, 103});
    const QList<int> all = drain(order);
    EXPECT_TRUE(all.contains(99));
    EXPECT_TRUE(all.contains(103));
    EXPECT_EQ(all.size(), 3);
}

TEST(ShuffleOrder, CandidateChangeKeepsHistoryThatStillExists) {
    ShuffleOrder order;
    order.reset(ids(5), 100);
    const int second = order.next(false);
    const int third = order.next(false);
    QList<int> kept = ids(5);
    kept.removeAll(second);
    order.setCandidates(kept);
    EXPECT_EQ(order.current(), third);
    EXPECT_EQ(order.previous(), 100);
    EXPECT_EQ(order.previous(), -1);
}

TEST(ShuffleOrder, RevisitingAPlayedItemKeepsEarlierHistory) {
    ShuffleOrder order;
    order.reset(ids(4), 100);
    const int second = order.next(false);
    const int third = order.next(false);
    order.setCurrent(second);
    EXPECT_EQ(order.current(), second);
    EXPECT_EQ(order.previous(), third);
    EXPECT_EQ(order.previous(), second);
    EXPECT_EQ(order.previous(), 100);
    EXPECT_EQ(order.previous(), -1);
}

TEST(ShuffleOrder, RemovingAnItemBetweenRepeatVisitsLeavesNoDuplicateStep) {
    ShuffleOrder order;
    order.reset(ids(3), 100);
    order.setCurrent(101);
    order.setCurrent(100);
    order.setCandidates({100, 102});
    EXPECT_EQ(order.current(), 100);
    EXPECT_FALSE(order.canGoBack());
}

TEST(ShuffleOrder, ManualSelectionIsNotReplayed) {
    ShuffleOrder order;
    order.reset(ids(6), 100);
    order.setCurrent(103);
    EXPECT_EQ(order.current(), 103);
    EXPECT_FALSE(drain(order).contains(103));
}

TEST(ShuffleOrder, SelectingAnIdOutsideTheCandidatesIsIgnored) {
    ShuffleOrder order;
    order.reset(ids(3), 100);
    order.setCurrent(999);
    EXPECT_EQ(order.current(), 100);
}

TEST(ShuffleOrder, ShrinkingTheCandidatesMidRoundNeverPlaysAnythingElse) {
    ShuffleOrder order;
    order.setSeed(7);
    QList<int> all;
    for (int id = 1; id <= 12; ++id) {
        all.append(id);
    }
    order.reset(all, 1);
    order.next(false);
    order.next(false);

    const QList<int> narrowed{2, 5, 8, 11};
    order.setCandidates(narrowed);
    EXPECT_LE(order.remaining(), narrowed.size());
    QSet<int> played;
    for (int id = order.next(false); id >= 0; id = order.next(false)) {
        EXPECT_TRUE(narrowed.contains(id)) << id;
        EXPECT_FALSE(played.contains(id)) << id;
        played.insert(id);
    }
    EXPECT_GE(played.size(), 2);
    for (int step = 0; step < 8; ++step) {
        const int id = order.next(true);
        EXPECT_TRUE(narrowed.contains(id)) << id;
    }
    for (int id = order.previous(); id >= 0; id = order.previous()) {
        EXPECT_TRUE(narrowed.contains(id)) << id;
    }
}

TEST(ShuffleOrder, GrowingTheCandidatesAddsThemToTheCurrentRound) {
    ShuffleOrder order;
    order.setSeed(11);
    order.reset({1, 2, 3}, 1);
    EXPECT_EQ(order.remaining(), 2);
    const int second = order.next(false);

    order.setCandidates({1, 2, 3, 4, 5, 6});
    EXPECT_EQ(order.remaining(), 4);
    QSet<int> played{1, second};
    for (int id = order.next(false); id >= 0; id = order.next(false)) {
        EXPECT_FALSE(played.contains(id)) << id;
        played.insert(id);
    }
    EXPECT_EQ(played, QSet<int>({1, 2, 3, 4, 5, 6}));
}
