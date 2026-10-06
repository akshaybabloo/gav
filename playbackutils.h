#ifndef PLAYBACKUTILS_H
#define PLAYBACKUTILS_H

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class PlaybackUtils : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    enum class TimeError { None, Malformed, OutOfRange };

    struct ParsedTime {
        bool ok = false;
        qint64 ms = 0;
        TimeError error = TimeError::Malformed;
    };

    struct Chapter {
        qint64 startMs = 0;
        qint64 endMs = 0;
        QString title;
    };

    static constexpr qint64 chapterPreviousThresholdMs = 3000;

    explicit PlaybackUtils(QObject *parent = nullptr);

    static ParsedTime parseTimeText(const QString &text, qint64 durationMs);
    static qint64 chapterTargetFor(const QList<Chapter> &chapters, qint64 positionMs, int direction);
    static bool resumeEligible(qint64 positionMs, qint64 durationMs);
    static QList<Chapter> chaptersFromVariant(const QVariantList &chapters);
    static QString localPathFor(const QString &pathOrUrl);

    Q_INVOKABLE QVariantMap parseTime(const QString &text, qint64 durationMs) const;
    Q_INVOKABLE qint64 chapterTarget(const QVariantList &chapters, qint64 positionMs, int direction) const;
    Q_INVOKABLE bool isResumeEligible(qint64 positionMs, qint64 durationMs) const;
    Q_INVOKABLE void revealInFileManager(const QString &pathOrUrl) const;
};

#endif // PLAYBACKUTILS_H
