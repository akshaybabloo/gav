#include <gtest/gtest.h>

#include "../playbackutils.h"

#include <limits>

namespace {

using Chapter = PlaybackUtils::Chapter;
using TimeError = PlaybackUtils::TimeError;

const QList<Chapter> threeChapters{{0, 10000, "One"}, {10000, 20000, "Two"}, {20000, 30000, "Three"}};

}

TEST(PlaybackUtilsParseTime, AcceptsPlainSeconds) {
    const auto parsed = PlaybackUtils::parseTimeText("65", 0);
    EXPECT_TRUE(parsed.ok);
    EXPECT_EQ(parsed.ms, 65000);
}

TEST(PlaybackUtilsParseTime, AcceptsMinutesSeconds) {
    EXPECT_EQ(PlaybackUtils::parseTimeText("1:05", 0).ms, 65000);
    EXPECT_EQ(PlaybackUtils::parseTimeText("01:05", 0).ms, 65000);
    EXPECT_EQ(PlaybackUtils::parseTimeText("83:45", 0).ms, (83 * 60 + 45) * 1000);
}

TEST(PlaybackUtilsParseTime, AcceptsHoursMinutesSeconds) {
    const auto parsed = PlaybackUtils::parseTimeText("1:02:03", 0);
    EXPECT_TRUE(parsed.ok);
    EXPECT_EQ(parsed.ms, (3600 + 2 * 60 + 3) * 1000);
}

TEST(PlaybackUtilsParseTime, AcceptsMilliseconds) {
    EXPECT_EQ(PlaybackUtils::parseTimeText("1:05.250", 0).ms, 65250);
    EXPECT_EQ(PlaybackUtils::parseTimeText("1:05.25", 0).ms, 65250);
    EXPECT_EQ(PlaybackUtils::parseTimeText("10.5", 0).ms, 10500);
}

TEST(PlaybackUtilsParseTime, TrimsWhitespace) {
    EXPECT_TRUE(PlaybackUtils::parseTimeText("  1:05 ", 0).ok);
}

TEST(PlaybackUtilsParseTime, RejectsOverflowingFields) {
    EXPECT_EQ(PlaybackUtils::parseTimeText("1:75", 0).error, TimeError::Malformed);
    EXPECT_EQ(PlaybackUtils::parseTimeText("9:99:99", 0).error, TimeError::Malformed);
    EXPECT_EQ(PlaybackUtils::parseTimeText("1:60:00", 0).error, TimeError::Malformed);
}

TEST(PlaybackUtilsParseTime, RejectsMalformedInput) {
    for (const char *text : {"", "abc", "-5", "1:2:3:4", "1:", ":30", "1.2.3", "1:05.1234"}) {
        const auto parsed = PlaybackUtils::parseTimeText(text, 0);
        EXPECT_FALSE(parsed.ok) << text;
        EXPECT_EQ(parsed.error, TimeError::Malformed) << text;
    }
}

TEST(PlaybackUtilsParseTime, RejectsBeyondDuration) {
    const auto parsed = PlaybackUtils::parseTimeText("2:00", 90000);
    EXPECT_FALSE(parsed.ok);
    EXPECT_EQ(parsed.error, TimeError::OutOfRange);
    EXPECT_TRUE(PlaybackUtils::parseTimeText("1:30", 90000).ok);
}

TEST(PlaybackUtilsParseTime, RejectsOversizedLeadingField) {
    const auto parsed = PlaybackUtils::parseTimeText("99999999999999999999", 0);
    EXPECT_FALSE(parsed.ok);
    EXPECT_EQ(parsed.error, TimeError::OutOfRange);
}

TEST(PlaybackUtilsParseTime, AcceptsMaximumRepresentableSeconds) {
    const auto parsed = PlaybackUtils::parseTimeText("9223372036854775.807", 0);
    EXPECT_TRUE(parsed.ok);
    EXPECT_EQ(parsed.ms, std::numeric_limits<qint64>::max());
}

TEST(PlaybackUtilsParseTime, RejectsArithmeticOverflow) {
    EXPECT_EQ(PlaybackUtils::parseTimeText("9223372036854775.808", 0).error, TimeError::OutOfRange);
    EXPECT_EQ(PlaybackUtils::parseTimeText("9223372036854776", 0).error, TimeError::OutOfRange);
    EXPECT_EQ(PlaybackUtils::parseTimeText("2562047788015216:00:00", 0).error, TimeError::OutOfRange);
}

TEST(PlaybackUtilsParseTime, VariantResultCarriesErrorCode) {
    PlaybackUtils utils;
    EXPECT_EQ(utils.parseTime("1:75", 0).value("error").toString(), "malformed");
    EXPECT_EQ(utils.parseTime("99", 1000).value("error").toString(), "outOfRange");
    const QVariantMap ok = utils.parseTime("1:05", 0);
    EXPECT_TRUE(ok.value("ok").toBool());
    EXPECT_EQ(ok.value("ms").toLongLong(), 65000);
    EXPECT_TRUE(ok.value("error").toString().isEmpty());
}

TEST(PlaybackUtilsChapters, NextGoesToFollowingChapterStart) {
    EXPECT_EQ(PlaybackUtils::chapterTargetFor(threeChapters, 0, 1), 10000);
    EXPECT_EQ(PlaybackUtils::chapterTargetFor(threeChapters, 12000, 1), 20000);
}

TEST(PlaybackUtilsChapters, NextDoesNothingInLastChapter) {
    EXPECT_EQ(PlaybackUtils::chapterTargetFor(threeChapters, 25000, 1), -1);
}

TEST(PlaybackUtilsChapters, PreviousGoesToCurrentChapterStart) {
    EXPECT_EQ(PlaybackUtils::chapterTargetFor(threeChapters, 15000, -1), 10000);
}

TEST(PlaybackUtilsChapters, PreviousWithinThresholdGoesToPreviousChapter) {
    EXPECT_EQ(PlaybackUtils::chapterTargetFor(threeChapters, 11000, -1), 0);
    EXPECT_EQ(PlaybackUtils::chapterTargetFor(threeChapters, 12999, -1), 0);
}

TEST(PlaybackUtilsChapters, PreviousAtExactlyThresholdStaysInCurrentChapter) {
    EXPECT_EQ(PlaybackUtils::chapterTargetFor(threeChapters, 13000, -1), 10000);
}

TEST(PlaybackUtilsChapters, PreviousInFirstChapterGoesToItsStart) {
    EXPECT_EQ(PlaybackUtils::chapterTargetFor(threeChapters, 1000, -1), 0);
    EXPECT_EQ(PlaybackUtils::chapterTargetFor(threeChapters, 0, -1), 0);
}

TEST(PlaybackUtilsChapters, NoChaptersGivesNoTarget) {
    EXPECT_EQ(PlaybackUtils::chapterTargetFor({}, 5000, 1), -1);
    EXPECT_EQ(PlaybackUtils::chapterTargetFor({}, 5000, -1), -1);
}

TEST(PlaybackUtilsChapters, UnsortedInputIsHandled) {
    const QList<Chapter> unsorted{{20000, 30000, "Three"}, {0, 10000, "One"}, {10000, 20000, "Two"}};
    EXPECT_EQ(PlaybackUtils::chapterTargetFor(unsorted, 5000, 1), 10000);
}

TEST(PlaybackUtilsChapters, VariantInput) {
    PlaybackUtils utils;
    const QVariantList chapters{QVariantMap{{"startMs", 0}, {"endMs", 10000}, {"title", "One"}},
                                QVariantMap{{"startMs", 10000}, {"endMs", 20000}, {"title", "Two"}}};
    EXPECT_EQ(utils.chapterTarget(chapters, 2000, 1), 10000);
}

TEST(PlaybackUtilsResume, WindowEdgesAreExclusive) {
    EXPECT_FALSE(PlaybackUtils::resumeEligible(5000, 100000));
    EXPECT_TRUE(PlaybackUtils::resumeEligible(5001, 100000));
    EXPECT_TRUE(PlaybackUtils::resumeEligible(94999, 100000));
    EXPECT_FALSE(PlaybackUtils::resumeEligible(95000, 100000));
}

TEST(PlaybackUtilsResume, UnknownDurationIsNotEligible) {
    EXPECT_FALSE(PlaybackUtils::resumeEligible(50000, 0));
    EXPECT_FALSE(PlaybackUtils::resumeEligible(0, 100000));
}
