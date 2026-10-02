#include <gtest/gtest.h>
#include "../subtitlefiles.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

class SubtitleFilesTest : public ::testing::Test {
protected:
    QString touch(const QString &name) {
        const QString path = QDir(tempDir.path()).filePath(name);
        QFile file(path);
        EXPECT_TRUE(file.open(QIODevice::WriteOnly));
        return path;
    }

    QStringList names(const QList<SubtitleFile> &files) {
        QStringList result;
        for (const SubtitleFile &file : files) {
            result.append(QFileInfo(file.path).fileName());
        }
        return result;
    }

    QTemporaryDir tempDir;
};

TEST_F(SubtitleFilesTest, MatchesPlainAndLanguageSuffixedSidecars) {
    const QString video = touch("movie.mkv");
    touch("movie.srt");
    touch("movie.en.srt");
    touch("movie.pt-BR.vtt");
    touch("movie.forced.ass");

    const QList<SubtitleFile> files = SubtitleFiles::discover(video);
    EXPECT_EQ(names(files), QStringList({"movie.en.srt", "movie.forced.ass", "movie.pt-BR.vtt", "movie.srt"}));
    EXPECT_EQ(files[0].language, "en");
    EXPECT_EQ(files[2].language, "pt-BR");
    EXPECT_TRUE(files[3].language.isEmpty());
}

#ifndef Q_OS_WIN
TEST_F(SubtitleFilesTest, MatchesCaseInsensitively) {
    const QString video = touch("movie.mkv");
    touch("MOVIE.EN.SRT");
    EXPECT_EQ(names(SubtitleFiles::discover(video)), QStringList({"MOVIE.EN.SRT"}));
}
#endif

TEST_F(SubtitleFilesTest, RejectsLongSuffixesOtherFilesAndOtherExtensions) {
    const QString video = touch("movie.mkv");
    touch("movie.toolongsuffix.srt");
    touch("other.srt");
    touch("movie.en.txt");
    touch("movie.en.us.srt");
    touch("moviex.srt");
    EXPECT_TRUE(SubtitleFiles::discover(video).isEmpty());
}

TEST_F(SubtitleFilesTest, ChoosesPreferredLanguage) {
    const QList<SubtitleFile> files{{"/v/movie.de.srt", "de"}, {"/v/movie.en.srt", "en"}, {"/v/movie.fr.srt", "fr"}};
    EXPECT_EQ(SubtitleFiles::choose(files, "fr"), 2);
    EXPECT_EQ(SubtitleFiles::choose(files, "fra"), 2);
    EXPECT_EQ(SubtitleFiles::choose(files, "eng"), 1);
}

TEST_F(SubtitleFilesTest, FallsBackToFirstAlphabetically) {
    const QList<SubtitleFile> files{{"/v/movie.de.srt", "de"}, {"/v/movie.en.srt", "en"}};
    EXPECT_EQ(SubtitleFiles::choose(files, "ja"), 0);
    EXPECT_EQ(SubtitleFiles::choose(files, ""), 0);
    EXPECT_EQ(SubtitleFiles::choose({}, "en"), -1);
}

TEST_F(SubtitleFilesTest, LanguageMatchingAcceptsTwoAndThreeLetterCodes) {
    EXPECT_TRUE(SubtitleFiles::languagesMatch("en", "eng"));
    EXPECT_TRUE(SubtitleFiles::languagesMatch("pt-BR", "por"));
    EXPECT_FALSE(SubtitleFiles::languagesMatch("en", "fr"));
    EXPECT_FALSE(SubtitleFiles::languagesMatch("", "en"));
}

TEST_F(SubtitleFilesTest, FixtureSidecarsAreFound) {
    const QList<SubtitleFile> files = SubtitleFiles::discover(QStringLiteral(GAV_TEST_DATA_DIR "/multi.mkv"));
    EXPECT_EQ(names(files), QStringList({"multi.en.srt", "multi.fr.srt"}));
}
