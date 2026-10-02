#include <gtest/gtest.h>
#include "../probemessage.h"
#include "../probeworker.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

TEST(ProbeProtocol, HelloRoundTrips) {
    const ProbeMessage parsed = ProbeMessage::parse(ProbeMessage::hello().serialise());
    EXPECT_EQ(parsed.type, ProbeMessage::Type::Hello);
    EXPECT_EQ(parsed.protocol, ProbeMessage::protocolVersion);
}

TEST(ProbeProtocol, MediaRoundTrips) {
    const ProbeMessage original = ProbeMessage::media(5400000, {{0, 300000, "Opening"}}, {{3, "ass", "eng", "Signs", false, true}},
                                                      {{"Fancy.ttf", "/tmp/fonts/Fancy.ttf"}});
    const ProbeMessage parsed = ProbeMessage::parse(original.serialise());
    ASSERT_EQ(parsed.type, ProbeMessage::Type::Media);
    EXPECT_EQ(parsed.durationMs, 5400000);
    ASSERT_EQ(parsed.chapters.size(), 1);
    EXPECT_EQ(parsed.chapters[0].title, "Opening");
    EXPECT_EQ(parsed.chapters[0].endMs, 300000);
    ASSERT_EQ(parsed.subtitleStreams.size(), 1);
    EXPECT_EQ(parsed.subtitleStreams[0].stream, 3);
    EXPECT_EQ(parsed.subtitleStreams[0].language, "eng");
    EXPECT_TRUE(parsed.subtitleStreams[0].forced);
    EXPECT_FALSE(parsed.subtitleStreams[0].isDefault);
    ASSERT_EQ(parsed.fonts.size(), 1);
    EXPECT_EQ(parsed.fonts[0].path, "/tmp/fonts/Fancy.ttf");
}

TEST(ProbeProtocol, HeaderEventEndRoundTrip) {
    const ProbeMessage header = ProbeMessage::parse(ProbeMessage::header("external:/a.srt", "[Script Info]\n", "WINDOWS-1252").serialise());
    EXPECT_EQ(header.type, ProbeMessage::Type::Header);
    EXPECT_EQ(header.source, "external:/a.srt");
    EXPECT_EQ(header.assHeader, "[Script Info]\n");
    EXPECT_EQ(header.encoding, "WINDOWS-1252");

    const ProbeMessage event =
        ProbeMessage::parse(ProbeMessage::eventFor("embedded:3", {61200, 2300, "0,0,Default,,0,0,0,,H\xc3\xa9llo"}).serialise());
    EXPECT_EQ(event.type, ProbeMessage::Type::Event);
    EXPECT_EQ(event.event.startMs, 61200);
    EXPECT_EQ(event.event.durationMs, 2300);
    EXPECT_EQ(QString::fromUtf8(event.event.ass), QString::fromUtf8("0,0,Default,,0,0,0,,H\xc3\xa9llo"));

    const ProbeMessage end = ProbeMessage::parse(ProbeMessage::end("embedded:3").serialise());
    EXPECT_EQ(end.type, ProbeMessage::Type::End);
    EXPECT_EQ(end.source, "embedded:3");
}

TEST(ProbeProtocol, ProgressWarningErrorRoundTrip) {
    EXPECT_EQ(ProbeMessage::parse(ProbeMessage::progress(1048576).serialise()).bytes, 1048576);

    const ProbeMessage warning = ProbeMessage::parse(ProbeMessage::warning("font-write-failed", "Fancy.ttf").serialise());
    EXPECT_EQ(warning.type, ProbeMessage::Type::Warning);
    EXPECT_EQ(warning.code, "font-write-failed");

    const ProbeMessage error = ProbeMessage::parse(ProbeMessage::error("open-failed", "Invalid data").serialise());
    EXPECT_EQ(error.type, ProbeMessage::Type::Error);
    EXPECT_EQ(error.detail, "Invalid data");
}

TEST(ProbeProtocol, SerialisedLinesHaveNoNewlines) {
    EXPECT_FALSE(ProbeMessage::header("embedded:1", "[Script Info]\nTitle: x\n").serialise().contains('\n'));
}

TEST(ProbeProtocol, UnknownTypeIsIgnoredNotInvalid) {
    const ProbeMessage parsed = ProbeMessage::parse(R"({"type":"future-thing","x":1})");
    EXPECT_TRUE(parsed.isValid());
    EXPECT_EQ(parsed.type, ProbeMessage::Type::Unknown);
}

TEST(ProbeProtocol, UnknownFieldsAreIgnored) {
    const ProbeMessage parsed = ProbeMessage::parse(R"({"type":"end","source":"embedded:2","extra":[1,2,3]})");
    EXPECT_EQ(parsed.type, ProbeMessage::Type::End);
    EXPECT_EQ(parsed.source, "embedded:2");
}

TEST(ProbeProtocol, NegativeEventTimesAreRejected) {
    EXPECT_FALSE(ProbeMessage::parse(R"({"type":"event","source":"embedded:2","startMs":-5,"durationMs":10,"ass":"x"})").isValid());
    EXPECT_FALSE(ProbeMessage::parse(R"({"type":"event","source":"embedded:2","startMs":5,"durationMs":-10,"ass":"x"})").isValid());
}

TEST(ProbeProtocol, WrongFieldTypesAreRejected) {
    EXPECT_FALSE(ProbeMessage::parse(R"({"type":"event","source":3,"startMs":5,"durationMs":10,"ass":"x"})").isValid());
    EXPECT_FALSE(ProbeMessage::parse(R"({"type":"progress","bytes":"many"})").isValid());
    EXPECT_FALSE(ProbeMessage::parse(R"({"type":"media","chapters":[{"startMs":10,"endMs":5}]})").isValid());
    EXPECT_FALSE(ProbeMessage::parse(R"({"type":"media","fonts":[{"name":"a.ttf"}]})").isValid());
    EXPECT_FALSE(ProbeMessage::parse(R"({"type":"end","source":""})").isValid());
}

TEST(ProbeProtocol, MalformedJsonIsInvalid) {
    EXPECT_FALSE(ProbeMessage::parse("{not json").isValid());
    EXPECT_FALSE(ProbeMessage::parse("[1,2]").isValid());
    EXPECT_FALSE(ProbeMessage::parse(R"({"x":1})").isValid());
    EXPECT_FALSE(ProbeMessage::parse("").isValid());
}


namespace {

QList<ProbeMessage> runProbe(const ProbeOptions &options, int *exitCode) {
    QList<ProbeMessage> messages;
    ProbeWorker worker([&messages](const ProbeMessage &message) { messages.append(ProbeMessage::parse(message.serialise())); });
    *exitCode = worker.run(options);
    return messages;
}

QList<ProbeMessage> ofType(const QList<ProbeMessage> &messages, ProbeMessage::Type type, const QString &source = {}) {
    QList<ProbeMessage> result;
    for (const ProbeMessage &message : messages) {
        if (message.type == type && (source.isEmpty() || message.source == source)) {
            result.append(message);
        }
    }
    return result;
}

const QString dataDir = QStringLiteral(GAV_TEST_DATA_DIR);

}

TEST(ProbeWorkerFixtures, ReportsMediaChaptersStreamsAndFonts) {
    QTemporaryDir fonts;
    ProbeOptions options;
    options.mediaPath = dataDir + "/multi.mkv";
    options.fontsDir = fonts.path();
    int exitCode = -1;
    const QList<ProbeMessage> messages = runProbe(options, &exitCode);

    EXPECT_EQ(exitCode, 0);
    ASSERT_FALSE(messages.isEmpty());
    EXPECT_EQ(messages.first().type, ProbeMessage::Type::Hello);

    const QList<ProbeMessage> media = ofType(messages, ProbeMessage::Type::Media);
    ASSERT_EQ(media.size(), 1);
    EXPECT_NEAR(media[0].durationMs, 30000, 100);
    ASSERT_EQ(media[0].chapters.size(), 3);
    EXPECT_EQ(media[0].chapters[0].title, "One");
    EXPECT_EQ(media[0].chapters[1].startMs, 10000);
    EXPECT_EQ(media[0].chapters[2].title, "Three");
    ASSERT_EQ(media[0].subtitleStreams.size(), 1);
    EXPECT_EQ(media[0].subtitleStreams[0].codec, "ass");
    EXPECT_EQ(media[0].subtitleStreams[0].language, "eng");
    EXPECT_EQ(media[0].subtitleStreams[0].title, "Signs");
    ASSERT_EQ(media[0].fonts.size(), 1);
    EXPECT_TRUE(QFile::exists(media[0].fonts[0].path));
    EXPECT_TRUE(media[0].fonts[0].path.startsWith(fonts.path()));
}

TEST(ProbeWorkerFixtures, StreamsEmbeddedAssEvents) {
    QTemporaryDir fonts;
    ProbeOptions options;
    options.mediaPath = dataDir + "/multi.mkv";
    options.fontsDir = fonts.path();
    int exitCode = -1;
    const QList<ProbeMessage> messages = runProbe(options, &exitCode);

    const QString source = QStringLiteral("embedded:3");
    const QList<ProbeMessage> headers = ofType(messages, ProbeMessage::Type::Header, source);
    ASSERT_EQ(headers.size(), 1);
    EXPECT_TRUE(headers[0].assHeader.contains("Style: Sign,DejaVu Serif"));

    const QList<ProbeMessage> events = ofType(messages, ProbeMessage::Type::Event, source);
    ASSERT_EQ(events.size(), 2);
    EXPECT_NEAR(events[0].event.startMs, 1000, 50);
    EXPECT_NEAR(events[0].event.durationMs, 8000, 50);
    EXPECT_TRUE(events[0].event.ass.contains("Signs track"));
    EXPECT_NEAR(events[1].event.startMs, 12000, 50);
    EXPECT_TRUE(events[1].event.ass.contains("\\frz15"));
    EXPECT_EQ(ofType(messages, ProbeMessage::Type::End, source).size(), 1);
}

TEST(ProbeWorkerFixtures, EventsNoneSkipsEmbeddedEvents) {
    QTemporaryDir fonts;
    ProbeOptions options;
    options.mediaPath = dataDir + "/multi.mkv";
    options.fontsDir = fonts.path();
    options.events = QStringLiteral("none");
    int exitCode = -1;
    const QList<ProbeMessage> messages = runProbe(options, &exitCode);
    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(ofType(messages, ProbeMessage::Type::Event).isEmpty());
}

TEST(ProbeWorkerFixtures, DecodesWindows1252Sidecar) {
    ProbeOptions options;
    options.subtitleFiles = {dataDir + "/multi.fr.srt"};
    int exitCode = -1;
    const QList<ProbeMessage> messages = runProbe(options, &exitCode);
    EXPECT_EQ(exitCode, 0);

    const QString source = QStringLiteral("external:") + QFileInfo(dataDir + "/multi.fr.srt").absoluteFilePath();
    const QList<ProbeMessage> headers = ofType(messages, ProbeMessage::Type::Header, source);
    ASSERT_EQ(headers.size(), 1);
    EXPECT_FALSE(headers[0].encoding.isEmpty());

    const QList<ProbeMessage> events = ofType(messages, ProbeMessage::Type::Event, source);
    ASSERT_EQ(events.size(), 2);
    EXPECT_TRUE(QString::fromUtf8(events[0].event.ass).contains(QString::fromUtf8("\xc3\x89t\xc3\xa9, d\xc3\xa9j\xc3\xa0 vu, gar\xc3\xa7on.")));
    EXPECT_EQ(events[1].event.startMs, 5000);
    EXPECT_EQ(events[1].event.durationMs, 3000);
    EXPECT_EQ(ofType(messages, ProbeMessage::Type::End, source).size(), 1);
}

TEST(ProbeWorkerFixtures, Utf8SidecarHasNoEncoding) {
    ProbeOptions options;
    options.subtitleFiles = {dataDir + "/multi.en.srt"};
    int exitCode = -1;
    const QList<ProbeMessage> messages = runProbe(options, &exitCode);
    const QList<ProbeMessage> headers = ofType(messages, ProbeMessage::Type::Header);
    ASSERT_EQ(headers.size(), 1);
    EXPECT_TRUE(headers[0].encoding.isEmpty());
}

TEST(ProbeWorkerFixtures, CorruptFileReportsOpenFailed) {
    ProbeOptions options;
    options.mediaPath = dataDir + "/corrupt.mkv";
    int exitCode = -1;
    const QList<ProbeMessage> messages = runProbe(options, &exitCode);
    EXPECT_EQ(exitCode, 1);
    ASSERT_FALSE(messages.isEmpty());
    EXPECT_EQ(messages.last().type, ProbeMessage::Type::Error);
    EXPECT_EQ(messages.last().code, "open-failed");
}

TEST(ProbeWorkerFixtures, MissingSidecarIsReportedPerSource) {
    ProbeOptions options;
    options.subtitleFiles = {dataDir + "/does-not-exist.srt", dataDir + "/multi.en.srt"};
    int exitCode = -1;
    const QList<ProbeMessage> messages = runProbe(options, &exitCode);
    EXPECT_EQ(exitCode, 0);
    const QList<ProbeMessage> warnings = ofType(messages, ProbeMessage::Type::Warning);
    ASSERT_EQ(warnings.size(), 1);
    EXPECT_EQ(warnings[0].code, "source-failed");
    EXPECT_TRUE(warnings[0].detail.contains("does-not-exist.srt"));
    EXPECT_EQ(ofType(messages, ProbeMessage::Type::End).size(), 1);
}

TEST(ProbeWorkerFixtures, DetectEncodingLeavesUtf8Alone) {
    EXPECT_TRUE(ProbeWorker::detectEncoding("plain ascii").isEmpty());
    EXPECT_TRUE(ProbeWorker::detectEncoding("caf\xc3\xa9").isEmpty());
    EXPECT_TRUE(ProbeWorker::detectEncoding("\xEF\xBB\xBFwith bom").isEmpty());
    EXPECT_FALSE(ProbeWorker::detectEncoding("caf\xe9 cr\xe8me br\xfbl\xe9" "e").isEmpty());
}

TEST(ProbeWorkerFixtures, SidecarsStillLoadWhenMediaCannotBeOpened) {
    ProbeOptions options;
    options.mediaPath = dataDir + "/corrupt.mkv";
    options.subtitleFiles = {dataDir + "/multi.en.srt"};
    int exitCode = -1;
    const QList<ProbeMessage> messages = runProbe(options, &exitCode);
    EXPECT_EQ(exitCode, 1);
    EXPECT_EQ(ofType(messages, ProbeMessage::Type::Event).size(), 2);
    EXPECT_EQ(messages.last().type, ProbeMessage::Type::Error);
}
