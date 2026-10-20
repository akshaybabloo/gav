#include <gtest/gtest.h>
#include "../rtspopengate.h"

#include <QSignalSpy>

TEST(RtspOpenGateTest, TheFirstRequestIsAdmittedAtOnce) {
    RtspOpenGate gate;
    EXPECT_TRUE(gate.request(1));
    EXPECT_TRUE(gate.holds(1));
    EXPECT_TRUE(gate.request(1));
}

TEST(RtspOpenGateTest, ASecondRequestWaitsForTheFirst) {
    RtspOpenGate gate;
    QSignalSpy admitted(&gate, &RtspOpenGate::admitted);
    gate.request(1);
    EXPECT_FALSE(gate.request(2));
    EXPECT_TRUE(gate.waiting(2));
    EXPECT_EQ(admitted.count(), 0);
    gate.release(1);
    ASSERT_EQ(admitted.count(), 1);
    EXPECT_EQ(admitted.at(0).at(0).toInt(), 2);
    EXPECT_TRUE(gate.holds(2));
    EXPECT_FALSE(gate.waiting(2));
}

TEST(RtspOpenGateTest, WaitingRequestsAreAdmittedInArrivalOrder) {
    RtspOpenGate gate;
    QSignalSpy admitted(&gate, &RtspOpenGate::admitted);
    gate.request(1);
    gate.request(3);
    gate.request(2);
    gate.request(3);
    gate.release(1);
    gate.release(3);
    gate.release(2);
    ASSERT_EQ(admitted.count(), 2);
    EXPECT_EQ(admitted.at(0).at(0).toInt(), 3);
    EXPECT_EQ(admitted.at(1).at(0).toInt(), 2);
    EXPECT_FALSE(gate.holds(2));
}

TEST(RtspOpenGateTest, CancellingAWaitingRequestRemovesIt) {
    RtspOpenGate gate;
    QSignalSpy admitted(&gate, &RtspOpenGate::admitted);
    gate.request(1);
    gate.request(2);
    gate.request(3);
    gate.cancel(2);
    EXPECT_FALSE(gate.waiting(2));
    gate.release(1);
    ASSERT_EQ(admitted.count(), 1);
    EXPECT_EQ(admitted.at(0).at(0).toInt(), 3);
}

TEST(RtspOpenGateTest, CancellingTheHolderAdmitsTheNext) {
    RtspOpenGate gate;
    gate.request(1);
    gate.request(2);
    gate.cancel(1);
    EXPECT_TRUE(gate.holds(2));
}

TEST(RtspOpenGateTest, ReleasingWithoutHoldingChangesNothing) {
    RtspOpenGate gate;
    gate.request(1);
    gate.request(2);
    gate.release(2);
    EXPECT_TRUE(gate.holds(1));
    EXPECT_TRUE(gate.waiting(2));
    gate.release(7);
    EXPECT_TRUE(gate.holds(1));
}

TEST(RtspOpenGateTest, AnOtherOpenDuringTheHoldMarksItOverlapped) {
    RtspOpenGate gate;
    gate.request(1);
    EXPECT_FALSE(gate.overlapped(1));
    gate.beginOtherOpen();
    gate.endOtherOpen();
    EXPECT_TRUE(gate.overlapped(1));
}

TEST(RtspOpenGateTest, AnOtherOpenAlreadyRunningMarksTheNewHolderOverlapped) {
    RtspOpenGate gate;
    gate.beginOtherOpen();
    gate.request(1);
    EXPECT_TRUE(gate.overlapped(1));
    gate.endOtherOpen();
    gate.release(1);
    gate.request(2);
    EXPECT_FALSE(gate.overlapped(2));
}

TEST(RtspOpenGateTest, AWaitingRequestAdmittedDuringAnOtherOpenIsOverlapped) {
    RtspOpenGate gate;
    gate.request(1);
    gate.request(2);
    gate.beginOtherOpen();
    gate.release(1);
    EXPECT_TRUE(gate.overlapped(2));
}

TEST(RtspOpenGateTest, AnOtherOpenFinishedBeforeTheHoldDoesNotCount) {
    RtspOpenGate gate;
    gate.beginOtherOpen();
    gate.endOtherOpen();
    gate.endOtherOpen();
    gate.request(1);
    EXPECT_FALSE(gate.overlapped(1));
    gate.beginOtherOpen();
    EXPECT_TRUE(gate.overlapped(1));
}

TEST(RtspOpenGateTest, OnlyTheHolderIsReportedOverlapped) {
    RtspOpenGate gate;
    gate.request(1);
    gate.request(2);
    gate.beginOtherOpen();
    EXPECT_TRUE(gate.overlapped(1));
    EXPECT_FALSE(gate.overlapped(2));
}
