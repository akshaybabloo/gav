#include <gtest/gtest.h>
#include "../credentialfiltersink.h"
#include "../streamaddress.h"

#include <spdlog/logger.h>
#include <spdlog/sinks/ostream_sink.h>

#include <sstream>

namespace {

QString display(const char *address) { return StreamAddress::displayAddress(QUrl(QString::fromLatin1(address))).toString(); }

std::string logged(const std::function<void(spdlog::logger &)> &write) {
    std::ostringstream output;
    spdlog::logger logger("test", std::make_shared<CredentialFilterSink>(std::make_shared<spdlog::sinks::ostream_sink_mt>(output)));
    logger.set_pattern("%v");
    write(logger);
    logger.flush();
    return output.str();
}

} // namespace

TEST(StreamAddressTest, RtspAddressesAreStreams) {
    EXPECT_TRUE(StreamAddress::isStream(QUrl("rtsp://cam.local/live")));
    EXPECT_TRUE(StreamAddress::isStream(QUrl("rtsps://cam.local:8322/live")));
    EXPECT_TRUE(StreamAddress::isStream(QUrl("RTSP://viewer:pw@192.168.1.20:554/stream1")));
    EXPECT_FALSE(StreamAddress::isStream(QUrl("rtsp:///live")));
    EXPECT_EQ(StreamAddress::schemes(), QStringList({"http", "https", "rtsp", "rtsps"}));
}

TEST(StreamAddressTest, HttpAndHttpsAreStreams) {
    EXPECT_TRUE(StreamAddress::isStream(QUrl("http://example.org/a.mp4")));
    EXPECT_TRUE(StreamAddress::isStream(QUrl("https://example.org/live.m3u8")));
    EXPECT_TRUE(StreamAddress::isStream(QUrl("HTTPS://example.org/live.m3u8")));
    EXPECT_TRUE(StreamAddress::isStream(QUrl("https://user:pw@example.org/live.m3u8")));
}

TEST(StreamAddressTest, OtherAddressesAreNotStreams) {
    EXPECT_FALSE(StreamAddress::isStream(QUrl("rtmp://example.org/live")));
    EXPECT_FALSE(StreamAddress::isStream(QUrl("mmsh://example.org/live")));
    EXPECT_FALSE(StreamAddress::isStream(QUrl("srt://example.org:9000")));
    EXPECT_FALSE(StreamAddress::isStream(QUrl("file:///home/user/a.mp4")));
    EXPECT_FALSE(StreamAddress::isStream(QUrl::fromLocalFile("/home/user/a.mp4")));
    EXPECT_FALSE(StreamAddress::isStream(QUrl("videos/a.mp4")));
    EXPECT_FALSE(StreamAddress::isStream(QUrl("https:///a.mp4")));
    EXPECT_FALSE(StreamAddress::isStream(QUrl()));
}

TEST(StreamAddressTest, RecognisesRtspSchemes) {
    EXPECT_TRUE(StreamAddress::isRtsp(QUrl("rtsp://cam.local/live")));
    EXPECT_TRUE(StreamAddress::isRtsp(QUrl("RTSP://cam.local/live")));
    EXPECT_TRUE(StreamAddress::isRtsp(QUrl("rtsps://cam.local/live")));
    EXPECT_FALSE(StreamAddress::isRtsp(QUrl("https://cam.local/live")));
    EXPECT_TRUE(StreamAddress::isSecureRtsp(QUrl("rtsps://cam.local/live")));
    EXPECT_FALSE(StreamAddress::isSecureRtsp(QUrl("rtsp://cam.local/live")));
}

TEST(StreamAddressTest, DisplayAddressLeavesCredentialsOut) {
    EXPECT_EQ(display("rtsp://viewer:s3cr3t@192.168.1.20:554/stream1"), "rtsp://192.168.1.20:554/stream1");
    EXPECT_EQ(display("rtsp://viewer@cam.local/live"), "rtsp://cam.local/live");
    EXPECT_EQ(display("rtsps://admin:p%40ss@cam.local/ch1?sub=1"), "rtsps://cam.local/ch1?sub=1");
    EXPECT_EQ(display("https://user:pw@example.org/a.m3u8"), "https://example.org/a.m3u8");
    EXPECT_EQ(display("rtsp://viewer:pw@[fe80::1]:8554/live"), "rtsp://[fe80::1]:8554/live");
}

TEST(StreamAddressTest, DisplayAddressLeavesOtherAddressesAlone) {
    const QUrl plain("rtsp://192.168.1.20/stream1");
    EXPECT_EQ(StreamAddress::displayAddress(plain), plain);
    const QUrl encoded("https://example.org/a%20b.m3u8?x=%2F#part");
    EXPECT_EQ(StreamAddress::displayAddress(encoded), encoded);
    const QUrl file = QUrl::fromLocalFile("/home/user/a@b.mp4");
    EXPECT_EQ(StreamAddress::displayAddress(file), file);
}

TEST(StreamAddressTest, DisplayTextHandlesPathsAndAddresses) {
    EXPECT_EQ(StreamAddress::displayText("https://user:pw@example.org/a.m3u8"), "https://example.org/a.m3u8");
    EXPECT_EQ(StreamAddress::displayText("https://example.org/a.m3u8"), "https://example.org/a.m3u8");
    EXPECT_EQ(StreamAddress::displayText("/home/user/a@b.mp4"), "/home/user/a@b.mp4");
    EXPECT_EQ(StreamAddress::displayText("C:/Videos/a@b.mp4"), "C:/Videos/a@b.mp4");
    EXPECT_EQ(StreamAddress::displayText(""), "");
}

TEST(StreamAddressTest, HasPasswordNeedsANonEmptyPassword) {
    EXPECT_TRUE(StreamAddress::hasPassword(QUrl("rtsp://viewer:s3cr3t@cam.local/live")));
    EXPECT_TRUE(StreamAddress::hasPassword(QUrl("rtsps://admin:p%40ss@cam.local/ch1")));
    EXPECT_TRUE(StreamAddress::hasPassword(QUrl("https://:token@example.org/a.m3u8")));
    EXPECT_FALSE(StreamAddress::hasPassword(QUrl("rtsp://viewer@cam.local/live")));
    EXPECT_FALSE(StreamAddress::hasPassword(QUrl("rtsp://viewer:@cam.local/live")));
    EXPECT_FALSE(StreamAddress::hasPassword(QUrl("rtsp://cam.local/live")));
    EXPECT_FALSE(StreamAddress::hasPassword(QUrl::fromLocalFile("/home/user/a:b@c.mp4")));
}

TEST(StreamAddressTest, SourceKeyIsHostAndPort) {
    EXPECT_EQ(StreamAddress::sourceKey(QUrl("rtsp://Cam.Local/live")), "cam.local:554");
    EXPECT_EQ(StreamAddress::sourceKey(QUrl("rtsp://cam.local:8554/other")), "cam.local:8554");
    EXPECT_EQ(StreamAddress::sourceKey(QUrl("rtsps://cam.local/live")), "cam.local:322");
    EXPECT_EQ(StreamAddress::sourceKey(QUrl("rtsps://cam.local:8322/live")), "cam.local:8322");
    EXPECT_EQ(StreamAddress::sourceKey(QUrl("rtsp://viewer:pw@cam.local/live")), StreamAddress::sourceKey(QUrl("rtsp://cam.local/else")));
}

TEST(StreamAddressTest, WithoutCredentialsCleansAddressesInsideText) {
    EXPECT_EQ(StreamAddress::withoutCredentials("nothing to see"), "nothing to see");
    EXPECT_EQ(StreamAddress::withoutCredentials("Loading source: 'rtsp://viewer:s3cr3t@cam.local/live'"), "Loading source: 'rtsp://cam.local/live'");
    EXPECT_EQ(StreamAddress::withoutCredentials("Input #0, rtsp, from 'rtsp://viewer:s3cr3tpw@127.0.0.1:8554/cam':"),
              "Input #0, rtsp, from 'rtsp://127.0.0.1:8554/cam':");
    EXPECT_EQ(StreamAddress::withoutCredentials("a https://u:p@one.org/x and b rtsp://v@two.org/y"), "a https://one.org/x and b rtsp://two.org/y");
    EXPECT_EQ(StreamAddress::withoutCredentials("rtsp://viewer:p@ss@cam.local/live"), "rtsp://cam.local/live");
}

TEST(StreamAddressTest, WithoutCredentialsLeavesOtherTextAlone) {
    EXPECT_EQ(StreamAddress::withoutCredentials("https://example.org/a.m3u8"), "https://example.org/a.m3u8");
    EXPECT_EQ(StreamAddress::withoutCredentials("https://example.org/users/me@example.org"), "https://example.org/users/me@example.org");
    EXPECT_EQ(StreamAddress::withoutCredentials("https://example.org?mail=me@example.org"), "https://example.org?mail=me@example.org");
    EXPECT_EQ(StreamAddress::withoutCredentials("see https://example.org or write to me@example.org"), "see https://example.org or write to me@example.org");
    EXPECT_EQ(StreamAddress::withoutCredentials("Connection to tcp://127.0.0.1:8599?timeout=20000000 failed"),
              "Connection to tcp://127.0.0.1:8599?timeout=20000000 failed");
}

TEST(CredentialFilterSinkTest, RemovesCredentialsFromPlainLines) {
    const std::string output = logged([](spdlog::logger &logger) { logger.info("Loading source: 'rtsp://viewer:s3cr3t@cam.local/live'"); });
    EXPECT_EQ(output.find("s3cr3t"), std::string::npos);
    EXPECT_EQ(output.find("viewer"), std::string::npos);
    EXPECT_NE(output.find("Loading source: 'rtsp://cam.local/live'"), std::string::npos);
}

TEST(CredentialFilterSinkTest, RemovesCredentialsFromFormattedLines) {
    const std::string address = "https://user:pw@example.org/a.m3u8";
    const std::string output = logged([&address](spdlog::logger &logger) { logger.warn("Stream {} failed after {} ms", address, 120); });
    EXPECT_EQ(output.find("user:pw"), std::string::npos);
    EXPECT_NE(output.find("Stream https://example.org/a.m3u8 failed after 120 ms"), std::string::npos);
}

TEST(CredentialFilterSinkTest, PassesOtherLinesThrough) {
    const std::string output = logged([](spdlog::logger &logger) {
        logger.info("plain line");
        logger.info("https://example.org/a.m3u8 opened");
    });
    EXPECT_NE(output.find("plain line"), std::string::npos);
    EXPECT_NE(output.find("https://example.org/a.m3u8 opened"), std::string::npos);
}
