#include "subtitlecontroller.h"
#include "subtitlefiles.h"

#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QtConcurrent/QtConcurrentRun>

#include <spdlog/spdlog.h>

#include <utility>

namespace {

QString stateName(SubtitleController::State state) {
    switch (state) {
    case SubtitleController::State::Pending:
        return QStringLiteral("pending");
    case SubtitleController::State::Streaming:
        return QStringLiteral("streaming");
    case SubtitleController::State::Complete:
        return QStringLiteral("complete");
    case SubtitleController::State::Failed:
        return QStringLiteral("failed");
    case SubtitleController::State::FallbackPlainText:
        return QStringLiteral("fallback");
    }
    return {};
}

}

SubtitleController::SubtitleController(QMediaPlayer *player, QObject *parent)
    : QObject(parent), m_player(player), m_probe(new MediaProbe(this)), m_renderer(new SubtitleRenderer(this)) {
    connect(m_probe, &MediaProbe::mediaReady, this, &SubtitleController::onMediaReady);
    connect(m_probe, &MediaProbe::headerReady, this, &SubtitleController::onHeaderReady);
    connect(m_probe, &MediaProbe::eventsReady, this, &SubtitleController::onEventsReady);
    connect(m_probe, &MediaProbe::sourceEnded, this, &SubtitleController::onSourceEnded);
    connect(m_probe, &MediaProbe::sourceFailed, this, &SubtitleController::onSourceFailed);
    connect(m_probe, &MediaProbe::failed, this, &SubtitleController::onProbeFailed);
}

void SubtitleController::setSource(const QUrl &source) {
    clear();
    if (!source.isLocalFile()) {
        return;
    }

    m_mediaPath = source.toLocalFile();
    const quint64 generation = m_generation;
    const QString path = m_mediaPath;
    QtConcurrent::run([path] { return SubtitleFiles::discover(path); })
        .then(this, [this, generation, path](const QList<SubtitleFile> &files) {
            if (generation != m_generation) {
                return;
            }
            QStringList paths;
            for (const SubtitleFile &file : files) {
                if (find(MediaProbe::externalSource(file.path))) {
                    continue;
                }
                Track track;
                track.id = MediaProbe::externalSource(file.path);
                track.path = file.path;
                track.language = file.language;
                track.title = QFileInfo(file.path).fileName();
                m_tracks.append(track);
                paths.append(file.path);
            }
            const int choice = SubtitleFiles::choose(files, m_preferredLanguage);
            m_sidecarChoice = choice >= 0 ? MediaProbe::externalSource(files[choice].path) : QString();
            if (!files.isEmpty()) {
                emit tracksChanged();
            }
            m_probe->start(path, paths);
        });
}

void SubtitleController::setStreamTracks(const QList<HlsSubtitle> &subtitles) {
    bool added = false;
    for (const HlsSubtitle &subtitle : subtitles) {
        const QString location = subtitle.url.toString();
        Track track;
        track.id = MediaProbe::externalSource(location);
        if (find(track.id)) {
            continue;
        }
        track.path = location;
        track.language = subtitle.language;
        track.title = subtitle.name;
        track.isDefault = subtitle.isDefault;
        track.forced = subtitle.forced;
        track.remote = true;
        m_tracks.append(track);
        added = true;
    }
    if (added) {
        emit tracksChanged();
    }
}

void SubtitleController::clear() {
    ++m_generation;
    m_probe->stop();
    m_renderer->reset();
    if (m_player) {
        m_player->setActiveSubtitleTrack(-1);
    }

    const bool hadTracks = !m_tracks.isEmpty();
    const bool hadActive = !m_activeId.isEmpty();
    const bool hadChapters = !m_chapters.isEmpty();
    m_tracks.clear();
    m_headers.clear();
    m_events.clear();
    m_chapters.clear();
    m_activeId.clear();
    m_mediaPath.clear();
    m_sidecarChoice.clear();
    m_userChoseTrack = false;
    m_mediaKnown = false;

    if (hadTracks) {
        emit tracksChanged();
    }
    if (hadActive) {
        emit activeTrackChanged();
    }
    if (hadChapters) {
        emit chaptersChanged();
    }
    setDelay(0);
}

QVariantList SubtitleController::tracks() const {
    QVariantList result;
    int ordinal = 0;
    for (const Track &track : m_tracks) {
        ++ordinal;
        result.append(QVariantMap{{QStringLiteral("id"), track.id},
                                  {QStringLiteral("origin"), track.embedded ? QStringLiteral("embedded") : QStringLiteral("external")},
                                  {QStringLiteral("language"), track.language},
                                  {QStringLiteral("title"), track.title},
                                  {QStringLiteral("displayName"), displayName(track, ordinal)},
                                  {QStringLiteral("codec"), track.codec},
                                  {QStringLiteral("loadState"), stateName(track.state)},
                                  {QStringLiteral("isDefault"), track.isDefault},
                                  {QStringLiteral("isForced"), track.forced}});
    }
    return result;
}

QString SubtitleController::activeTrackId() const { return m_activeId; }

QString SubtitleController::activeTrackName() const {
    if (m_activeId.isEmpty()) {
        return tr("Off");
    }
    for (qsizetype i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i].id == m_activeId) {
            return displayName(m_tracks[i], int(i) + 1);
        }
    }
    return {};
}

int SubtitleController::delay() const { return m_delayMs; }

void SubtitleController::setDelay(int delayMs) {
    const int clamped = qBound(-delayLimitMs, delayMs, delayLimitMs);
    if (clamped == m_delayMs) {
        return;
    }
    m_delayMs = clamped;
    emit delayChanged();
}

int SubtitleController::adjustDelay(int deltaMs) {
    setDelay(m_delayMs + deltaMs);
    return m_delayMs;
}

qreal SubtitleController::scale() const { return m_scale; }

void SubtitleController::setScale(qreal scale) {
    const qreal clamped = qBound(scaleMin, qRound(scale * 10.0) / 10.0, scaleMax);
    if (qFuzzyCompare(clamped, m_scale)) {
        return;
    }
    m_scale = clamped;
    m_renderer->setScale(m_scale);
    emit scaleChanged();
}

QString SubtitleController::preferredLanguage() const { return m_preferredLanguage; }

void SubtitleController::setPreferredLanguage(const QString &language) {
    const QString trimmed = language.trimmed();
    if (trimmed == m_preferredLanguage) {
        return;
    }
    m_preferredLanguage = trimmed;
    emit preferredLanguageChanged();
}

QVariantList SubtitleController::chapters() const { return m_chapters; }

SubtitleRenderer *SubtitleController::renderer() const { return m_renderer; }

QString SubtitleController::languageName(const QString &code) {
    if (code.isEmpty()) {
        return {};
    }
    const QString primary = code.section(QLatin1Char('-'), 0, 0).section(QLatin1Char('_'), 0, 0);
    const QLocale::Language language = QLocale::codeToLanguage(primary, QLocale::AnyLanguageCode);
    return language == QLocale::AnyLanguage ? code : QLocale::languageToString(language);
}

QString SubtitleController::displayName(const Track &track, int ordinal) {
    const QString language = languageName(track.language);
    if (!track.title.isEmpty() && !language.isEmpty()) {
        return QStringLiteral("%1 (%2)").arg(track.title, language);
    }
    if (!track.title.isEmpty()) {
        return track.title;
    }
    if (!language.isEmpty()) {
        return language;
    }
    return tr("Track %1").arg(ordinal);
}

void SubtitleController::selectTrack(const QString &id) { activate(id, true); }

QString SubtitleController::cycleTrack() {
    QStringList ids;
    for (const Track &track : std::as_const(m_tracks)) {
        if (!(track.state == State::Failed && !track.embedded)) {
            ids.append(track.id);
        }
    }
    ids.append(QString());
    const qsizetype current = ids.indexOf(m_activeId);
    activate(ids[(current + 1) % ids.size()], true);
    return activeTrackName();
}

bool SubtitleController::loadFile(const QUrl &url) {
    if (!url.isLocalFile() || !SubtitleFiles::isSubtitleFile(url.toLocalFile())) {
        return false;
    }
    const QString path = QFileInfo(url.toLocalFile()).absoluteFilePath();
    if (!QFileInfo::exists(path)) {
        return false;
    }

    const QString id = MediaProbe::externalSource(path);
    Track *existing = find(id);
    if (!existing || existing->state == State::Failed) {
        if (existing) {
            existing->state = State::Pending;
        } else {
            Track track;
            track.id = id;
            track.path = path;
            track.title = QFileInfo(path).fileName();
            m_tracks.append(track);
        }
        m_headers.remove(id);
        m_events.remove(id);
        emit tracksChanged();
        m_probe->loadSubtitleFile(path);
    }
    activate(id, true);
    return true;
}

void SubtitleController::activate(const QString &id, bool userChoice) {
    if (userChoice) {
        m_userChoseTrack = true;
    }
    Track *track = find(id);
    if (!id.isEmpty() && !track) {
        return;
    }
    if (track && track->remote && !track->requested) {
        track->requested = true;
        m_probe->loadSubtitleFile(track->path);
    }
    m_activeId = id;
    applyActiveTrack();
    emit activeTrackChanged();
}

void SubtitleController::applyActiveTrack() {
    if (m_player) {
        m_player->setActiveSubtitleTrack(-1);
    }
    const Track *track = find(m_activeId);
    if (!track) {
        m_renderer->clearTrack();
        return;
    }

    if (track->state == State::Failed || track->state == State::FallbackPlainText) {
        m_renderer->clearTrack();
        const int qtIndex = track->embedded ? qtSubtitleIndex(*track) : -1;
        if (qtIndex >= 0 && m_player) {
            m_player->setActiveSubtitleTrack(qtIndex);
            setState(track->id, State::FallbackPlainText);
        }
        return;
    }

    const auto header = m_headers.constFind(track->id);
    if (header == m_headers.cend()) {
        m_renderer->clearTrack();
        return;
    }
    m_renderer->setTrack(*header);
    m_renderer->addEvents(m_events.value(track->id));
}

void SubtitleController::onMediaReady(const ProbeMessage &media) {
    m_chapters.clear();
    int index = 0;
    for (const ProbeChapter &chapter : media.chapters) {
        m_chapters.append(QVariantMap{{QStringLiteral("index"), index++},
                                      {QStringLiteral("startMs"), chapter.startMs},
                                      {QStringLiteral("endMs"), chapter.endMs},
                                      {QStringLiteral("title"), chapter.title}});
    }
    emit chaptersChanged();

    QList<Track> embedded;
    for (const ProbeSubtitleStream &stream : media.subtitleStreams) {
        Track track;
        track.id = QStringLiteral("embedded:%1").arg(stream.stream);
        track.embedded = true;
        track.stream = stream.stream;
        track.language = stream.language;
        track.title = stream.title;
        track.codec = stream.codec;
        track.isDefault = stream.isDefault;
        track.forced = stream.forced;
        if (!find(track.id)) {
            embedded.append(track);
        }
    }
    m_tracks = embedded + m_tracks;
    m_mediaKnown = true;
    emit tracksChanged();

    loadFonts(media.fonts);
    autoSelect();
}

void SubtitleController::loadFonts(const QList<ProbeFont> &fonts) {
    if (fonts.isEmpty()) {
        return;
    }
    const quint64 generation = m_generation;
    QtConcurrent::run([fonts] {
        QList<std::pair<QString, QByteArray>> loaded;
        for (const ProbeFont &font : fonts) {
            QFile file(font.path);
            if (file.open(QIODevice::ReadOnly)) {
                loaded.append({font.name, file.readAll()});
            }
        }
        return loaded;
    }).then(this, [this, generation](const QList<std::pair<QString, QByteArray>> &loaded) {
        if (generation != m_generation) {
            return;
        }
        for (const auto &[name, data] : loaded) {
            m_renderer->addFont(name, data);
        }
    });
}

void SubtitleController::onHeaderReady(const QString &source, const QString &assHeader) {
    if (!find(source)) {
        return;
    }
    m_headers.insert(source, assHeader.toUtf8());
    setState(source, State::Streaming);
    if (source == m_activeId) {
        applyActiveTrack();
    }
}

void SubtitleController::onEventsReady(const QString &source, const QList<ProbeEvent> &events) {
    if (!find(source)) {
        return;
    }
    m_events[source].append(events);
    if (source == m_activeId && m_headers.contains(source)) {
        m_renderer->addEvents(events);
    }
}

void SubtitleController::onSourceEnded(const QString &source) { setState(source, State::Complete); }

void SubtitleController::onSourceFailed(const QString &source, const QString &detail) {
    spdlog::warn("Subtitle source {} failed: {}", source.toStdString(), detail.toStdString());
    markFailed(source);
    if (const Track *track = find(source); track && !track->embedded) {
        emit errorOccurred(track->remote ? tr("Could not load the stream's subtitles") : tr("Could not read subtitle file %1").arg(QFileInfo(track->path).fileName()));
    }
}

void SubtitleController::onProbeFailed(const QStringList &sources, const QString &code, const QString &detail) {
    Q_UNUSED(detail)
    bool affectedActive = false;
    for (const QString &source : sources) {
        markFailed(source);
        affectedActive = affectedActive || source == m_activeId;
    }
    if (!m_mediaKnown) {
        m_mediaKnown = true;
        autoSelect();
    }
    if (affectedActive || (!sources.isEmpty() && code != QLatin1String("open-failed"))) {
        const Track *active = find(m_activeId);
        emit errorOccurred(active && active->state == State::FallbackPlainText ? tr("Subtitles could not be fully read; showing plain text")
                                                                                 : tr("Subtitles could not be read"));
    }
}

void SubtitleController::markFailed(const QString &source) {
    Track *track = find(source);
    if (!track || track->state == State::Complete) {
        return;
    }
    setState(source, State::Failed);
    if (source == m_activeId) {
        applyActiveTrack();
        emit activeTrackChanged();
    }
}

void SubtitleController::autoSelect() {
    if (m_userChoseTrack) {
        return;
    }

    QString choice;
    for (const Track &track : std::as_const(m_tracks)) {
        if (SubtitleFiles::languagesMatch(track.language, m_preferredLanguage)) {
            choice = track.id;
            break;
        }
    }
    if (choice.isEmpty()) {
        for (const Track &track : std::as_const(m_tracks)) {
            if (track.embedded && (track.forced || track.isDefault)) {
                choice = track.id;
                break;
            }
        }
    }
    if (choice.isEmpty() && find(m_sidecarChoice)) {
        choice = m_sidecarChoice;
    }
    if (choice != m_activeId) {
        activate(choice, false);
    }
}

int SubtitleController::qtSubtitleIndex(const Track &track) const {
    if (!m_player) {
        return -1;
    }
    const QList<QMediaMetaData> qtTracks = m_player->subtitleTracks();
    for (qsizetype i = 0; i < qtTracks.size(); ++i) {
        const QString title = qtTracks[i].stringValue(QMediaMetaData::Title);
        const QVariant language = qtTracks[i].value(QMediaMetaData::Language);
        const bool titleMatches = !track.title.isEmpty() && title == track.title;
        const bool languageMatches = language.isValid() && !track.language.isEmpty() &&
                                     SubtitleFiles::languagesMatch(QLocale::languageToCode(language.value<QLocale::Language>()), track.language);
        if (titleMatches && (languageMatches || track.language.isEmpty())) {
            return int(i);
        }
    }
    int ordinal = 0;
    for (const Track &candidate : m_tracks) {
        if (!candidate.embedded) {
            continue;
        }
        if (candidate.id == track.id) {
            return ordinal < qtTracks.size() ? ordinal : -1;
        }
        ++ordinal;
    }
    return -1;
}

SubtitleController::Track *SubtitleController::find(const QString &id) {
    for (Track &track : m_tracks) {
        if (track.id == id) {
            return &track;
        }
    }
    return nullptr;
}

const SubtitleController::Track *SubtitleController::find(const QString &id) const {
    for (const Track &track : m_tracks) {
        if (track.id == id) {
            return &track;
        }
    }
    return nullptr;
}

void SubtitleController::setState(const QString &id, State state) {
    Track *track = find(id);
    if (!track || track->state == state) {
        return;
    }
    track->state = state;
    emit tracksChanged();
}
