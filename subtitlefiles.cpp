#include "subtitlefiles.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>

#include <algorithm>

namespace SubtitleFiles {

const QStringList &extensions() {
    static const QStringList list{QStringLiteral("srt"), QStringLiteral("ass"), QStringLiteral("ssa"), QStringLiteral("vtt")};
    return list;
}

bool isSubtitleFile(const QString &path) { return extensions().contains(QFileInfo(path).suffix().toLower()); }

QList<SubtitleFile> discover(const QString &videoPath) {
    const QFileInfo video(videoPath);
    const QString base = video.completeBaseName();
    const QDir directory = video.absoluteDir();
    if (base.isEmpty()) {
        return {};
    }

    QList<SubtitleFile> result;
    const QFileInfoList entries = directory.entryInfoList(QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &entry : entries) {
        if (!extensions().contains(entry.suffix().toLower())) {
            continue;
        }
        const QString stem = entry.completeBaseName();
        if (stem.compare(base, Qt::CaseInsensitive) == 0) {
            result.append({entry.absoluteFilePath(), QString()});
            continue;
        }
        if (stem.size() <= base.size() + 1 || !stem.startsWith(base, Qt::CaseInsensitive) || stem.at(base.size()) != QLatin1Char('.')) {
            continue;
        }
        const QString language = stem.mid(base.size() + 1);
        if (language.isEmpty() || language.size() > 8 || language.contains(QLatin1Char('.'))) {
            continue;
        }
        result.append({entry.absoluteFilePath(), language});
    }

    std::sort(result.begin(), result.end(), [](const SubtitleFile &a, const SubtitleFile &b) {
        return QFileInfo(a.path).fileName().compare(QFileInfo(b.path).fileName(), Qt::CaseInsensitive) < 0;
    });
    return result;
}

bool languagesMatch(const QString &a, const QString &b) {
    if (a.isEmpty() || b.isEmpty()) {
        return false;
    }
    if (a.compare(b, Qt::CaseInsensitive) == 0) {
        return true;
    }
    const auto toLanguage = [](const QString &code) {
        const QString primary = code.section(QLatin1Char('-'), 0, 0).section(QLatin1Char('_'), 0, 0).toLower();
        return QLocale::codeToLanguage(primary, QLocale::AnyLanguageCode);
    };
    const QLocale::Language first = toLanguage(a);
    return first != QLocale::AnyLanguage && first == toLanguage(b);
}

int choose(const QList<SubtitleFile> &files, const QString &preferredLanguage) {
    if (files.isEmpty()) {
        return -1;
    }
    for (qsizetype i = 0; i < files.size(); ++i) {
        if (languagesMatch(files[i].language, preferredLanguage)) {
            return int(i);
        }
    }
    return 0;
}

}
