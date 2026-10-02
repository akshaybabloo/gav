#include "playbackutils.h"

#include <QRegularExpression>
#include <QtNumeric>

#include <algorithm>

PlaybackUtils::PlaybackUtils(QObject *parent) : QObject(parent) {}

PlaybackUtils::ParsedTime PlaybackUtils::parseTimeText(const QString &text, qint64 durationMs) {
    static const QRegularExpression pattern(QStringLiteral(R"(^(\d+)(?::(\d{1,2}))?(?::(\d{1,2}))?(?:\.(\d{1,3}))?$)"));

    ParsedTime result;
    const QRegularExpressionMatch match = pattern.match(text.trimmed());
    if (!match.hasMatch()) {
        return result;
    }

    bool converted = false;
    QList<qint64> fields{match.captured(1).toLongLong(&converted)};
    if (!converted) {
        result.error = TimeError::OutOfRange;
        return result;
    }
    if (match.hasCaptured(2)) {
        fields.append(match.captured(2).toLongLong());
    }
    if (match.hasCaptured(3)) {
        fields.append(match.captured(3).toLongLong());
    }

    for (qsizetype i = 1; i < fields.size(); ++i) {
        if (fields[i] >= 60) {
            return result;
        }
    }

    qint64 seconds = fields.first();
    for (qsizetype i = 1; i < fields.size(); ++i) {
        if (qMulOverflow(seconds, qint64(60), &seconds) || qAddOverflow(seconds, fields[i], &seconds)) {
            result.error = TimeError::OutOfRange;
            return result;
        }
    }

    qint64 millis = 0;
    if (match.hasCaptured(4)) {
        millis = match.captured(4).leftJustified(3, QLatin1Char('0')).toLongLong();
    }

    qint64 ms = 0;
    if (qMulOverflow(seconds, qint64(1000), &ms) || qAddOverflow(ms, millis, &ms)) {
        result.error = TimeError::OutOfRange;
        return result;
    }
    if (durationMs > 0 && ms > durationMs) {
        result.error = TimeError::OutOfRange;
        return result;
    }

    result.ok = true;
    result.ms = ms;
    result.error = TimeError::None;
    return result;
}

qint64 PlaybackUtils::chapterTargetFor(const QList<Chapter> &chapters, qint64 positionMs, int direction) {
    if (chapters.isEmpty() || direction == 0) {
        return -1;
    }

    QList<Chapter> sorted = chapters;
    std::sort(sorted.begin(), sorted.end(), [](const Chapter &a, const Chapter &b) { return a.startMs < b.startMs; });

    if (direction > 0) {
        for (const Chapter &chapter : sorted) {
            if (chapter.startMs > positionMs) {
                return chapter.startMs;
            }
        }
        return -1;
    }

    qsizetype current = -1;
    for (qsizetype i = 0; i < sorted.size(); ++i) {
        if (sorted[i].startMs <= positionMs) {
            current = i;
        }
    }
    if (current < 0) {
        return -1;
    }
    if (positionMs - sorted[current].startMs < chapterPreviousThresholdMs && current > 0) {
        return sorted[current - 1].startMs;
    }
    return sorted[current].startMs;
}

bool PlaybackUtils::resumeEligible(qint64 positionMs, qint64 durationMs) {
    if (durationMs <= 0 || positionMs <= 0) {
        return false;
    }
    return positionMs * 100 > durationMs * 5 && positionMs * 100 < durationMs * 95;
}

QList<PlaybackUtils::Chapter> PlaybackUtils::chaptersFromVariant(const QVariantList &chapters) {
    QList<Chapter> result;
    result.reserve(chapters.size());
    for (const QVariant &value : chapters) {
        const QVariantMap map = value.toMap();
        result.append({map.value(QStringLiteral("startMs")).toLongLong(), map.value(QStringLiteral("endMs")).toLongLong(),
                       map.value(QStringLiteral("title")).toString()});
    }
    return result;
}

QVariantMap PlaybackUtils::parseTime(const QString &text, qint64 durationMs) const {
    const ParsedTime parsed = parseTimeText(text, durationMs);
    QString error;
    switch (parsed.error) {
    case TimeError::None:
        break;
    case TimeError::Malformed:
        error = QStringLiteral("malformed");
        break;
    case TimeError::OutOfRange:
        error = QStringLiteral("outOfRange");
        break;
    }
    return {{QStringLiteral("ok"), parsed.ok}, {QStringLiteral("ms"), parsed.ms}, {QStringLiteral("error"), error}};
}

qint64 PlaybackUtils::chapterTarget(const QVariantList &chapters, qint64 positionMs, int direction) const {
    return chapterTargetFor(chaptersFromVariant(chapters), positionMs, direction);
}

bool PlaybackUtils::isResumeEligible(qint64 positionMs, qint64 durationMs) const {
    return resumeEligible(positionMs, durationMs);
}
