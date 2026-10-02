#ifndef SUBTITLEFILES_H
#define SUBTITLEFILES_H

#include <QList>
#include <QString>
#include <QStringList>

struct SubtitleFile {
    QString path;
    QString language;
};

namespace SubtitleFiles {

const QStringList &extensions();
bool isSubtitleFile(const QString &path);
QList<SubtitleFile> discover(const QString &videoPath);
int choose(const QList<SubtitleFile> &files, const QString &preferredLanguage);
bool languagesMatch(const QString &a, const QString &b);

}

#endif // SUBTITLEFILES_H
