#include <gtest/gtest.h>
#include "../rtspconnection.h"

extern "C" {
#include <libavutil/error.h>
}

namespace {

using Failure = RtspConnection::Failure;
using Kind = RtspConnection::Step::Kind;
using Setting = RtspConnection::Setting;
using State = RtspConnection::State;
using Step = RtspConnection::Step;
using Transport = RtspConnection::Transport;

const QUrl camera("rtsp://cam.local/live");
const QUrl secureCamera("rtsps://cam.local/live");

QString ffmpegText(int code) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(code, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

RtspConnection playing(Setting setting = Setting::Tcp) {
    RtspConnection connection;
    connection.open(camera, setting, 0);
    connection.firstPicture();
    return connection;
}

} // namespace

class RtspConnectionTest : public ::testing::Test {
protected:
    void SetUp() override { RtspConnection::resetTransportMemory(); }
};

class RtspReconnectTest : public RtspConnectionTest {};

TEST_F(RtspConnectionTest, AutomaticStartsWithUdp) {
    RtspConnection connection;
    const Step step = connection.open(camera, Setting::Automatic, 1000);
    EXPECT_EQ(step.kind, Kind::Open);
    EXPECT_EQ(step.transport, Transport::Udp);
    EXPECT_EQ(step.networkTimeoutMs, 5000);
    EXPECT_EQ(connection.state(), State::Connecting);
    EXPECT_EQ(connection.transportInUse(), Transport::None);
    EXPECT_EQ(connection.deadlineMs(), 6000);
}

TEST_F(RtspConnectionTest, UdpAttemptSucceeds) {
    RtspConnection connection;
    connection.open(camera, Setting::Automatic, 0);
    EXPECT_EQ(connection.firstPicture().kind, Kind::None);
    EXPECT_EQ(connection.state(), State::Playing);
    EXPECT_EQ(connection.transportInUse(), Transport::Udp);
    EXPECT_EQ(connection.deadlineMs(), -1);
}

TEST_F(RtspConnectionTest, UdpErrorIsFollowedByTcp) {
    RtspConnection connection;
    connection.open(camera, Setting::Automatic, 0);
    const Step step = connection.playerError(Failure::Other, 300);
    EXPECT_EQ(step.kind, Kind::Open);
    EXPECT_EQ(step.transport, Transport::Tcp);
    EXPECT_EQ(connection.state(), State::Connecting);
    EXPECT_EQ(connection.deadlineMs(), 300 + 12000);
    connection.firstPicture();
    EXPECT_EQ(connection.transportInUse(), Transport::Tcp);
}

TEST_F(RtspConnectionTest, UdpWithNothingForFiveSecondsIsFollowedByTcp) {
    RtspConnection connection;
    connection.open(camera, Setting::Automatic, 0);
    EXPECT_EQ(connection.timePassed(4999).kind, Kind::None);
    const Step step = connection.timePassed(5000);
    EXPECT_EQ(step.kind, Kind::Open);
    EXPECT_EQ(step.transport, Transport::Tcp);
}

TEST_F(RtspConnectionTest, LastAttemptErrorFails) {
    RtspConnection connection;
    connection.open(camera, Setting::Automatic, 0);
    connection.timePassed(5000);
    const Step step = connection.playerError(Failure::Other, 15000);
    EXPECT_EQ(step.kind, Kind::Fail);
    EXPECT_EQ(step.failure, Failure::Other);
    EXPECT_EQ(step.transport, Transport::Tcp);
    EXPECT_FALSE(step.forced);
    EXPECT_EQ(connection.state(), State::Failed);
    EXPECT_EQ(connection.deadlineMs(), -1);
}

TEST_F(RtspConnectionTest, LastAttemptGivesUpAfterTwelveSeconds) {
    RtspConnection connection;
    connection.open(camera, Setting::Tcp, 1000);
    EXPECT_EQ(connection.timePassed(12999).kind, Kind::None);
    const Step step = connection.timePassed(13000);
    EXPECT_EQ(step.kind, Kind::Fail);
    EXPECT_EQ(step.failure, Failure::Other);
}

TEST_F(RtspConnectionTest, SignInFailureStopsWithoutTryingTcp) {
    RtspConnection connection;
    connection.open(camera, Setting::Automatic, 0);
    const Step step = connection.playerError(Failure::SignIn, 200);
    EXPECT_EQ(step.kind, Kind::Fail);
    EXPECT_EQ(step.failure, Failure::SignIn);
    EXPECT_EQ(step.transport, Transport::Udp);
    EXPECT_EQ(connection.state(), State::Failed);
}

TEST_F(RtspConnectionTest, NotFoundStopsWithoutTryingTcp) {
    RtspConnection connection;
    connection.open(camera, Setting::Automatic, 0);
    const Step step = connection.playerError(Failure::NotFound, 200);
    EXPECT_EQ(step.kind, Kind::Fail);
    EXPECT_EQ(step.failure, Failure::NotFound);
}

TEST_F(RtspConnectionTest, SecureAddressesUseTcpWhateverTheSetting) {
    for (const Setting setting : {Setting::Automatic, Setting::Tcp, Setting::Udp}) {
        RtspConnection connection;
        const Step step = connection.open(secureCamera, setting, 0);
        EXPECT_EQ(step.transport, Transport::Tcp);
        const Step failed = connection.playerError(Failure::Other, 100);
        EXPECT_EQ(failed.kind, Kind::Fail);
        EXPECT_FALSE(failed.forced);
    }
}

TEST_F(RtspConnectionTest, CloseCancelsFromEveryState) {
    RtspConnection connection;
    connection.open(camera, Setting::Automatic, 0);
    connection.close();
    EXPECT_EQ(connection.state(), State::Idle);
    EXPECT_EQ(connection.timePassed(60000).kind, Kind::None);
    EXPECT_EQ(connection.playerError(Failure::Other, 60000).kind, Kind::None);

    connection = playing();
    connection.close();
    EXPECT_EQ(connection.state(), State::Idle);
    EXPECT_EQ(connection.transportInUse(), Transport::None);
    EXPECT_EQ(connection.lost(100).kind, Kind::None);

    connection = playing();
    connection.lost(100);
    connection.close();
    EXPECT_EQ(connection.state(), State::Idle);
    EXPECT_EQ(connection.timePassed(60000).kind, Kind::None);
}

TEST_F(RtspConnectionTest, ForcedTcpMakesOneTcpAttempt) {
    RtspConnection connection;
    EXPECT_EQ(connection.open(camera, Setting::Tcp, 0).transport, Transport::Tcp);
    const Step step = connection.playerError(Failure::Other, 100);
    EXPECT_EQ(step.kind, Kind::Fail);
    EXPECT_EQ(step.transport, Transport::Tcp);
    EXPECT_TRUE(step.forced);
}

TEST_F(RtspConnectionTest, ForcedUdpNeverFallsBack) {
    RtspConnection connection;
    EXPECT_EQ(connection.open(camera, Setting::Udp, 0).transport, Transport::Udp);
    EXPECT_EQ(connection.timePassed(5000).kind, Kind::None);
    const Step step = connection.playerError(Failure::Other, 10000);
    EXPECT_EQ(step.kind, Kind::Fail);
    EXPECT_EQ(step.failure, Failure::Other);
    EXPECT_EQ(step.transport, Transport::Udp);
    EXPECT_TRUE(step.forced);
}

TEST_F(RtspConnectionTest, RemembersForTheSessionThatASourceNeededTcp) {
    RtspConnection connection;
    connection.open(camera, Setting::Automatic, 0);
    connection.playerError(Failure::Other, 100);
    connection.firstPicture();
    connection.close();

    EXPECT_EQ(connection.open(QUrl("rtsp://viewer:pw@CAM.local/other"), Setting::Automatic, 0).transport, Transport::Tcp);
    const Step failed = connection.playerError(Failure::Other, 100);
    EXPECT_EQ(failed.kind, Kind::Fail);
    EXPECT_FALSE(failed.forced);

    EXPECT_EQ(connection.open(QUrl("rtsp://cam.local:8554/live"), Setting::Automatic, 0).transport, Transport::Udp);
    EXPECT_EQ(connection.open(QUrl("rtsp://other.local/live"), Setting::Automatic, 0).transport, Transport::Udp);
}

TEST_F(RtspConnectionTest, ForcedTcpDoesNotFillTheMemory) {
    RtspConnection connection;
    connection.open(camera, Setting::Tcp, 0);
    connection.firstPicture();
    connection.close();
    EXPECT_EQ(connection.open(camera, Setting::Automatic, 0).transport, Transport::Udp);
}

TEST_F(RtspConnectionTest, ForgettingTransportsEmptiesTheMemory) {
    RtspConnection connection;
    connection.open(camera, Setting::Automatic, 0);
    connection.playerError(Failure::Other, 100);
    connection.firstPicture();
    connection.close();
    connection.forgetTransports();
    EXPECT_EQ(connection.open(camera, Setting::Automatic, 0).transport, Transport::Udp);
}

TEST_F(RtspConnectionTest, AnAttemptsTimeStartsWhenItReallyStarts) {
    RtspConnection connection;
    connection.open(camera, Setting::Automatic, 0);
    connection.attemptStarted(3000);
    EXPECT_EQ(connection.deadlineMs(), 8000);
    EXPECT_EQ(connection.timePassed(7999).kind, Kind::None);
    EXPECT_EQ(connection.timePassed(8000).transport, Transport::Tcp);
}

TEST_F(RtspConnectionTest, AttemptStartedIsIgnoredWhenNotOpening) {
    RtspConnection connection = playing();
    connection.attemptStarted(3000);
    EXPECT_EQ(connection.deadlineMs(), -1);
}

TEST_F(RtspConnectionTest, ConnectionsShareWhatTheyLearnedAboutTcp) {
    RtspConnection first;
    first.open(camera, Setting::Automatic, 0);
    first.playerError(Failure::Other, 100);
    first.firstPicture();

    RtspConnection second;
    EXPECT_EQ(second.open(camera, Setting::Automatic, 0).transport, Transport::Tcp);
}

TEST_F(RtspConnectionTest, ForgettingTransportsOnOneConnectionForgetsForAll) {
    RtspConnection first;
    first.open(camera, Setting::Automatic, 0);
    first.playerError(Failure::Other, 100);
    first.firstPicture();

    RtspConnection second;
    second.forgetTransports();
    EXPECT_EQ(first.open(camera, Setting::Automatic, 0).transport, Transport::Udp);
    EXPECT_EQ(second.open(camera, Setting::Automatic, 0).transport, Transport::Udp);
}

TEST_F(RtspConnectionTest, UnknownSettingValuesAreAutomatic) {
    EXPECT_EQ(RtspConnection::settingFor("auto"), Setting::Automatic);
    EXPECT_EQ(RtspConnection::settingFor("tcp"), Setting::Tcp);
    EXPECT_EQ(RtspConnection::settingFor("udp"), Setting::Udp);
    EXPECT_EQ(RtspConnection::settingFor(""), Setting::Automatic);
    EXPECT_EQ(RtspConnection::settingFor("TCP"), Setting::Automatic);
    EXPECT_EQ(RtspConnection::settingFor("multicast"), Setting::Automatic);
    EXPECT_EQ(RtspConnection::settingName(Setting::Udp), "udp");
    EXPECT_EQ(RtspConnection::transportName(Transport::Tcp), "TCP");
    EXPECT_EQ(RtspConnection::transportName(Transport::None), "");
}

TEST(RtspConnectionFailureTest, MapsFfmpegDescriptions) {
    EXPECT_EQ(RtspConnection::failureFor(ffmpegText(AVERROR_HTTP_UNAUTHORIZED)), Failure::SignIn);
    EXPECT_EQ(RtspConnection::failureFor(ffmpegText(AVERROR_HTTP_FORBIDDEN)), Failure::SignIn);
    EXPECT_EQ(RtspConnection::failureFor(ffmpegText(AVERROR_HTTP_NOT_FOUND)), Failure::NotFound);
    EXPECT_EQ(RtspConnection::failureFor(ffmpegText(AVERROR_HTTP_BAD_REQUEST)), Failure::NotFound);
    EXPECT_EQ(RtspConnection::failureFor(ffmpegText(AVERROR(ECONNREFUSED))), Failure::Other);
    EXPECT_EQ(RtspConnection::failureFor(ffmpegText(AVERROR(ETIMEDOUT))), Failure::Other);
    EXPECT_EQ(RtspConnection::failureFor(ffmpegText(AVERROR(EPROTONOSUPPORT))), Failure::Other);
    EXPECT_EQ(RtspConnection::failureFor(QString()), Failure::Other);
}

TEST(RtspConnectionFailureTest, MatchesWholeStatusNumbersOnly) {
    EXPECT_EQ(RtspConnection::failureFor("Could not open media. FFmpeg error description: Server returned 401 Unauthorized (authorization failed)"),
              Failure::SignIn);
    EXPECT_EQ(RtspConnection::failureFor("Error number -4010 occurred"), Failure::Other);
    EXPECT_EQ(RtspConnection::failureFor("Connection to tcp://127.0.0.1:4040 failed"), Failure::Other);
}

TEST(RtspConnectionFailureTest, KeepsTheLatestOpenFailureUntilTaken) {
    RtspConnection::takeOpenFailure();
    RtspConnection::noteOpenFailure("first");
    RtspConnection::noteOpenFailure("second");
    EXPECT_EQ(RtspConnection::takeOpenFailure(), "second");
    EXPECT_TRUE(RtspConnection::takeOpenFailure().isEmpty());
}

TEST_F(RtspReconnectTest, ALostStreamWaitsTwoSecondsThenReconnects) {
    RtspConnection connection = playing(Setting::Udp);
    const Step wait = connection.lost(10000);
    EXPECT_EQ(wait.kind, Kind::Wait);
    EXPECT_EQ(connection.state(), State::Reconnecting);
    EXPECT_EQ(connection.reconnectAttempt(), 1);
    EXPECT_EQ(connection.deadlineMs(), 12000);
    EXPECT_EQ(connection.timePassed(11999).kind, Kind::None);
    const Step attempt = connection.timePassed(12000);
    EXPECT_EQ(attempt.kind, Kind::Open);
    EXPECT_EQ(attempt.transport, Transport::Udp);
    EXPECT_EQ(attempt.networkTimeoutMs, 5000);
    connection.firstPicture();
    EXPECT_EQ(connection.state(), State::Playing);
    EXPECT_EQ(connection.transportInUse(), Transport::Udp);
    EXPECT_EQ(connection.reconnectAttempt(), 0);
}

TEST_F(RtspReconnectTest, QuickFailuresKeepToTheSchedule) {
    RtspConnection connection = playing();
    connection.lost(0);
    connection.timePassed(2000);
    EXPECT_EQ(connection.playerError(Failure::Other, 2100).kind, Kind::Wait);
    EXPECT_EQ(connection.reconnectAttempt(), 2);
    EXPECT_EQ(connection.deadlineMs(), 9000);
    EXPECT_EQ(connection.timePassed(9000).kind, Kind::Open);
    EXPECT_EQ(connection.playerError(Failure::Other, 9100).kind, Kind::Wait);
    EXPECT_EQ(connection.reconnectAttempt(), 3);
    EXPECT_EQ(connection.deadlineMs(), 16000);
    EXPECT_EQ(connection.timePassed(16000).kind, Kind::Open);
    connection.firstPicture();
    EXPECT_EQ(connection.state(), State::Playing);
}

TEST_F(RtspReconnectTest, SlowFailuresStartTheNextAttemptAtOnce) {
    RtspConnection connection = playing();
    connection.lost(0);
    connection.timePassed(2000);
    EXPECT_EQ(connection.deadlineMs(), 14000);
    EXPECT_EQ(connection.timePassed(14000).kind, Kind::Wait);
    EXPECT_EQ(connection.deadlineMs(), 14000);
    EXPECT_EQ(connection.timePassed(14000).kind, Kind::Open);
    EXPECT_EQ(connection.timePassed(26000).kind, Kind::Wait);
    EXPECT_EQ(connection.timePassed(26000).kind, Kind::Open);
    const Step step = connection.timePassed(38000);
    EXPECT_EQ(step.kind, Kind::Fail);
    EXPECT_EQ(step.failure, Failure::Lost);
}

TEST_F(RtspReconnectTest, ThreeFailedAttemptsLoseTheStream) {
    RtspConnection connection = playing();
    connection.lost(0);
    connection.timePassed(2000);
    connection.playerError(Failure::Other, 2100);
    connection.timePassed(9000);
    connection.playerError(Failure::Other, 9100);
    connection.timePassed(16000);
    const Step step = connection.playerError(Failure::Other, 16100);
    EXPECT_EQ(step.kind, Kind::Fail);
    EXPECT_EQ(step.failure, Failure::Lost);
    EXPECT_EQ(step.transport, Transport::Tcp);
    EXPECT_EQ(connection.state(), State::Failed);
    EXPECT_EQ(connection.transportInUse(), Transport::None);
}

TEST_F(RtspReconnectTest, ARefusedSignInStopsReconnecting) {
    RtspConnection connection = playing();
    connection.lost(0);
    connection.timePassed(2000);
    const Step step = connection.playerError(Failure::SignIn, 2100);
    EXPECT_EQ(step.kind, Kind::Fail);
    EXPECT_EQ(step.failure, Failure::SignIn);
}

TEST_F(RtspReconnectTest, ErrorsWhileWaitingAreIgnored) {
    RtspConnection connection = playing();
    connection.lost(0);
    EXPECT_EQ(connection.playerError(Failure::Other, 500).kind, Kind::None);
    EXPECT_EQ(connection.lost(600).kind, Kind::None);
    EXPECT_EQ(connection.reconnectAttempt(), 1);
    EXPECT_EQ(connection.deadlineMs(), 2000);
}

TEST_F(RtspReconnectTest, AnErrorWhilePlayingIsALoss) {
    RtspConnection connection = playing();
    EXPECT_EQ(connection.playerError(Failure::Other, 5000).kind, Kind::Wait);
    EXPECT_EQ(connection.state(), State::Reconnecting);
}

TEST_F(RtspReconnectTest, LossIsIgnoredBeforeTheStreamPlayed) {
    RtspConnection connection;
    EXPECT_EQ(connection.lost(0).kind, Kind::None);
    connection.open(camera, Setting::Tcp, 0);
    EXPECT_EQ(connection.lost(100).kind, Kind::None);
    EXPECT_EQ(connection.ended(false, 100).kind, Kind::None);
    EXPECT_EQ(connection.state(), State::Connecting);
}

TEST_F(RtspReconnectTest, AStreamLostWhilePausedWaitsForPlay) {
    RtspConnection connection = playing();
    EXPECT_EQ(connection.lost(1000, true).kind, Kind::None);
    EXPECT_EQ(connection.state(), State::Idle);
    EXPECT_TRUE(connection.reopenOnPlay());
    EXPECT_EQ(connection.deadlineMs(), -1);
    connection.open(camera, Setting::Tcp, 2000);
    EXPECT_FALSE(connection.reopenOnPlay());
}

TEST_F(RtspReconnectTest, TheEndOfALiveStreamIsALoss) {
    RtspConnection connection = playing();
    EXPECT_EQ(connection.ended(false, 1000).kind, Kind::Wait);
    EXPECT_EQ(connection.state(), State::Reconnecting);
}

TEST_F(RtspReconnectTest, TheEndOfARecordingIsItsEnd) {
    RtspConnection connection = playing();
    EXPECT_EQ(connection.ended(true, 1000).kind, Kind::Ended);
    EXPECT_EQ(connection.state(), State::Playing);
    EXPECT_FALSE(connection.reopenOnPlay());
    EXPECT_EQ(connection.deadlineMs(), -1);
    EXPECT_EQ(connection.timePassed(60000).kind, Kind::None);
}
