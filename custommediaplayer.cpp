#include "custommediaplayer.h"
#include "playbackutils.h"
#include "previewimageprovider.h"
#include "streamquality.h"
#include "subtitlefiles.h"
#include <QQuickWindow>
#include <QScreen>
#include <QVideoSink>
#include <QVideoFrame>
#include <QImage>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QBuffer>
#include <QTimer>
#include <QDebug>
#include <QFileInfo>
#include <QLocale>
#include <QMediaFormat>
#include <QPlaybackOptions>
#include <QVideoFrameFormat>

#include <spdlog/spdlog.h>

#include <chrono>
#include <memory>
#include <utility>

CustomMediaPlayer::CustomMediaPlayer() {
  m_mediaPlayer = new QMediaPlayer(this);

  // Forward signals from QMediaPlayer
  connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this,
          &CustomMediaPlayer::playbackStateChanged);
  connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this,
          &CustomMediaPlayer::mediaStatusChanged);
  connect(m_mediaPlayer, &QMediaPlayer::tracksChanged, this, &CustomMediaPlayer::updateHasVideo);
  connect(m_mediaPlayer, &QMediaPlayer::playbackRateChanged, this, &CustomMediaPlayer::playbackRateChanged);


  connect(m_mediaPlayer, &QMediaPlayer::durationChanged, this,
          &CustomMediaPlayer::durationChanged);
  connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this,
          &CustomMediaPlayer::positionChanged);
  connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this,
          &CustomMediaPlayer::onStatusChanged);

  connect(m_mediaPlayer,
          QOverload<QMediaPlayer::Error, const QString &>::of(
            &QMediaPlayer::errorOccurred),
          this, &CustomMediaPlayer::onMediaPlayerError);

  m_fpsTimer = new QTimer(this);
  m_fpsTimer->setInterval(1000);
  connect(m_fpsTimer, &QTimer::timeout, this, &CustomMediaPlayer::onFpsTick);

  connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
      if (state == QMediaPlayer::PlayingState) {
          if (m_statsEnabled) {
              m_frameCount = 0;
              resetDroppedFrameWindow();
              m_fpsTimer->start();
          }
      } else {
          m_fpsTimer->stop();
          if (m_fps != 0) {
              m_fps = 0;
              emit fpsChanged();
          }
      }
  });

  connect(m_mediaPlayer, &QMediaPlayer::metaDataChanged, this, &CustomMediaPlayer::updateMediaInfo);
  connect(m_mediaPlayer, &QMediaPlayer::tracksChanged, this, &CustomMediaPlayer::updateMediaInfo);
  connect(m_mediaPlayer, &QMediaPlayer::activeTracksChanged, this, &CustomMediaPlayer::updateMediaInfo);

  connect(m_mediaPlayer, &QMediaPlayer::seekableChanged, this, &CustomMediaPlayer::liveChanged);
  connect(m_mediaPlayer, &QMediaPlayer::durationChanged, this, &CustomMediaPlayer::liveChanged);
  connect(this, &CustomMediaPlayer::sourceChanged, this, &CustomMediaPlayer::liveChanged);
  connect(this, &CustomMediaPlayer::mediaLoadedChanged, this, &CustomMediaPlayer::liveChanged);
  connect(m_mediaPlayer, &QMediaPlayer::metaDataChanged, this, &CustomMediaPlayer::nowPlayingChanged);
  connect(this, &CustomMediaPlayer::sourceChanged, this, &CustomMediaPlayer::nowPlayingChanged);

  QPlaybackOptions playbackOptions = m_mediaPlayer->playbackOptions();
  playbackOptions.setNetworkTimeout(std::chrono::milliseconds(streamNetworkTimeoutMs));
  m_mediaPlayer->setPlaybackOptions(playbackOptions);

  m_quality = new StreamQuality(this);
  connect(m_quality, &StreamQuality::changed, this, &CustomMediaPlayer::qualitiesChanged);
  connect(m_quality, &StreamQuality::resolved, this, &CustomMediaPlayer::onQualityResolved);
  connect(m_quality, &StreamQuality::subtitlesFound, this, [this](const QList<HlsSubtitle> &subtitles) { m_streamSubtitles = subtitles; });
  connect(m_quality, &StreamQuality::failed, this, &CustomMediaPlayer::onQualityFailed);

  m_streamLoadTimer = new QTimer(this);
  m_streamLoadTimer->setSingleShot(true);
  m_streamLoadTimer->setInterval(streamLoadTimeoutMs);
  connect(m_streamLoadTimer, &QTimer::timeout, this, &CustomMediaPlayer::onStreamLoadTimeout);

  m_stallTimer = new QTimer(this);
  m_stallTimer->setInterval(stallPollIntervalMs);
  connect(m_stallTimer, &QTimer::timeout, this, &CustomMediaPlayer::checkForStall);
  connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
    m_stallPosition = -1;
    if (state == QMediaPlayer::PlayingState) {
      m_stallTimer->start();
    } else {
      m_stallTimer->stop();
      setBuffering(false);
    }
  });

  m_subtitles = new SubtitleController(m_mediaPlayer, this);
  connect(m_subtitles, &SubtitleController::chaptersChanged, this, &CustomMediaPlayer::chaptersChanged);
  connect(m_mediaPlayer, &QMediaPlayer::tracksChanged, this, &CustomMediaPlayer::audioTracksChanged);
  connect(m_mediaPlayer, &QMediaPlayer::activeTracksChanged, this, &CustomMediaPlayer::audioTracksChanged);
}

SubtitleController *CustomMediaPlayer::subtitles() const { return m_subtitles; }

bool CustomMediaPlayer::seekable() const { return m_mediaLoaded && m_mediaPlayer->isSeekable(); }

bool CustomMediaPlayer::isLive() const {
  const QUrl current = m_source;
  return m_mediaLoaded && !current.isEmpty() && !current.isLocalFile() && m_mediaPlayer->duration() <= 0 && !m_mediaPlayer->isSeekable();
}

QString CustomMediaPlayer::mediaTitle() const { return m_mediaPlayer->metaData().stringValue(QMediaMetaData::Title); }

QString CustomMediaPlayer::mediaArtist() const {
  const QMediaMetaData metaData = m_mediaPlayer->metaData();
  const QString artist = metaData.stringValue(QMediaMetaData::ContributingArtist);
  return artist.isEmpty() ? metaData.stringValue(QMediaMetaData::AlbumArtist) : artist;
}

QString CustomMediaPlayer::mediaAlbum() const { return m_mediaPlayer->metaData().stringValue(QMediaMetaData::AlbumTitle); }

QImage CustomMediaPlayer::coverArt() const { return m_mediaPlayer->metaData().value(QMediaMetaData::CoverArtImage).value<QImage>(); }

bool CustomMediaPlayer::buffering() const { return m_buffering; }

void CustomMediaPlayer::setBuffering(bool buffering) {
  if (m_buffering == buffering)
    return;
  m_buffering = buffering;
  emit bufferingChanged();
}

void CustomMediaPlayer::checkForStall() {
  const qint64 current = m_mediaPlayer->position();
  const qint64 duration = m_mediaPlayer->duration();
  const bool atEnd = m_mediaPlayer->mediaStatus() == QMediaPlayer::EndOfMedia || (duration > 0 && current >= duration);
  if (current != m_stallPosition || atEnd || !m_mediaLoaded) {
    m_stallPosition = current;
    m_stallClock.start();
    setBuffering(false);
    return;
  }
  if (m_stallClock.isValid() && m_stallClock.elapsed() >= stallThresholdMs) {
    setBuffering(true);
  }
}

void CustomMediaPlayer::onStreamLoadTimeout() {
  if (m_mediaLoaded || m_source.isEmpty())
    return;
  qWarning() << "Stream did not load within" << streamLoadTimeoutMs << "ms:" << m_source;
  emit errorOccurred(tr("The stream took too long to open. Check the address and your connection."));
  stop();
}

double CustomMediaPlayer::frameDurationUs() const {
  double frameRate = 0;
  if (QVideoSink *sink = m_mediaPlayer->videoSink()) {
    frameRate = sink->videoFrame().streamFrameRate();
  }
  if (frameRate <= 0) {
    const QList<QMediaMetaData> tracks = m_mediaPlayer->videoTracks();
    const int active = m_mediaPlayer->activeVideoTrack();
    if (active >= 0 && active < tracks.size()) {
      frameRate = tracks[active].value(QMediaMetaData::VideoFrameRate).toDouble();
    }
  }
  if (frameRate <= 0) {
    frameRate = m_mediaPlayer->metaData().value(QMediaMetaData::VideoFrameRate).toDouble();
  }
  return 1e6 / (frameRate > 0 ? frameRate : 25.0);
}

bool CustomMediaPlayer::stepFrame(int direction) {
  if (!m_hasVideo || !m_mediaLoaded || direction == 0)
    return false;
  if (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
    m_mediaPlayer->pause();
  }

  QVideoSink *sink = m_mediaPlayer->videoSink();
  const qint64 displayedUs = sink && sink->videoFrame().isValid() && sink->videoFrame().startTime() >= 0
      ? sink->videoFrame().startTime()
      : m_mediaPlayer->position() * 1000;
  const qint64 frameStartUs = m_stepBaseUs >= 0 ? m_stepBaseUs : displayedUs;
  const double frameUs = frameDurationUs();
  if (direction < 0 && frameStartUs < frameUs / 2)
    return false;

  const qint64 expectedUs = qMax<qint64>(0, qRound64(frameStartUs + (direction > 0 ? frameUs : -frameUs)));
  const qint64 targetMs = qBound<qint64>(0, qRound64((expectedUs + frameUs * 0.5) / 1000.0), m_mediaPlayer->duration());
  spdlog::debug("stepFrame {}: frame at {} us, frame duration {:.0f} us, seeking to {} ms", direction, frameStartUs, frameUs, targetMs);

  m_stepBaseUs = expectedUs;
  if (!m_stepTimer) {
    m_stepTimer = new QTimer(this);
    m_stepTimer->setSingleShot(true);
    m_stepTimer->setInterval(1000);
    connect(m_stepTimer, &QTimer::timeout, this, &CustomMediaPlayer::clearPendingStep);
  }
  m_stepTimer->start();
  if (sink && !m_stepConnection) {
    m_stepConnection = connect(sink, &QVideoSink::videoFrameChanged, this, [this, frameUs](const QVideoFrame &frame) {
      if (!frame.isValid() || m_stepBaseUs < 0)
        return;
      spdlog::debug("stepFrame result: frame at {} us (expected {} us)", frame.startTime(), m_stepBaseUs);
      if (qAbs(frame.startTime() - m_stepBaseUs) < frameUs * 0.5)
        clearPendingStep();
    });
  }

  resetDroppedFrameWindow();
  m_mediaPlayer->setPosition(targetMs);
  return true;
}

void CustomMediaPlayer::clearPendingStep() {
  m_stepBaseUs = -1;
  if (m_stepConnection) {
    QObject::disconnect(m_stepConnection);
    m_stepConnection = {};
  }
  if (m_stepTimer)
    m_stepTimer->stop();
}

QString CustomMediaPlayer::jumpChapter(int direction) {
  const QList<PlaybackUtils::Chapter> chapters = PlaybackUtils::chaptersFromVariant(m_subtitles->chapters());
  const qint64 target = PlaybackUtils::chapterTargetFor(chapters, m_mediaPlayer->position(), direction);
  if (target < 0)
    return {};
  setPosition(target);
  for (const PlaybackUtils::Chapter &chapter : chapters) {
    if (chapter.startMs == target)
      return chapter.title;
  }
  return {};
}

QString CustomMediaPlayer::nextChapter() { return jumpChapter(1); }

QString CustomMediaPlayer::previousChapter() { return jumpChapter(-1); }

void CustomMediaPlayer::checkpoint() {
  const QUrl current = m_source;
  if (current.isEmpty() || !m_mediaLoaded || m_mediaPlayer->duration() <= 0)
    return;
  emit positionCheckpoint(current, m_mediaPlayer->position(), m_mediaPlayer->duration());
}

QVariantList CustomMediaPlayer::chapters() const { return m_subtitles->chapters(); }

QString CustomMediaPlayer::audioTrackName(int index) const {
  const QList<QMediaMetaData> tracks = m_mediaPlayer->audioTracks();
  if (index < 0 || index >= tracks.size()) {
    return {};
  }
  SubtitleController::Track track;
  track.title = tracks[index].stringValue(QMediaMetaData::Title);
  const QVariant language = tracks[index].value(QMediaMetaData::Language);
  if (language.isValid() && language.value<QLocale::Language>() != QLocale::AnyLanguage) {
    track.language = QLocale::languageToCode(language.value<QLocale::Language>());
  }
  return SubtitleController::displayName(track, index + 1);
}

QVariantList CustomMediaPlayer::audioTracks() const {
  QVariantList result;
  const qsizetype count = m_mediaPlayer->audioTracks().size();
  for (qsizetype i = 0; i < count; ++i) {
    result.append(QVariantMap{{QStringLiteral("index"), int(i)}, {QStringLiteral("displayName"), audioTrackName(int(i))}});
  }
  return result;
}

int CustomMediaPlayer::activeAudioTrack() const {
  return m_pendingAudioTrack >= 0 ? m_pendingAudioTrack : m_mediaPlayer->activeAudioTrack();
}

QString CustomMediaPlayer::activeAudioTrackName() const { return audioTrackName(activeAudioTrack()); }

QString CustomMediaPlayer::preferredAudioLanguage() const { return m_preferredAudioLanguage; }

void CustomMediaPlayer::setPreferredAudioLanguage(const QString &language) {
  const QString trimmed = language.trimmed();
  if (trimmed == m_preferredAudioLanguage)
    return;
  m_preferredAudioLanguage = trimmed;
  emit preferredAudioLanguageChanged();
}

void CustomMediaPlayer::selectAudioTrack(int index) {
  if (m_mediaPlayer->mediaStatus() < QMediaPlayer::LoadedMedia) {
    m_pendingAudioTrack = index;
    emit audioTracksChanged();
    return;
  }
  if (index < 0 || index >= m_mediaPlayer->audioTracks().size() || index == m_mediaPlayer->activeAudioTrack())
    return;
  m_mediaPlayer->setActiveAudioTrack(index);
}

QString CustomMediaPlayer::cycleAudioTrack() {
  const int count = int(m_mediaPlayer->audioTracks().size());
  if (count < 2)
    return activeAudioTrackName();
  const int next = (qMax(0, activeAudioTrack()) + 1) % count;
  selectAudioTrack(next);
  return audioTrackName(next);
}

void CustomMediaPlayer::applyAudioSelection() {
  if (m_audioSelectionApplied)
    return;
  m_audioSelectionApplied = true;

  int index = m_pendingAudioTrack;
  m_pendingAudioTrack = -1;
  const QList<QMediaMetaData> tracks = m_mediaPlayer->audioTracks();
  if (index < 0 && !m_preferredAudioLanguage.isEmpty()) {
    for (qsizetype i = 0; i < tracks.size(); ++i) {
      const QVariant language = tracks[i].value(QMediaMetaData::Language);
      if (language.isValid() &&
          SubtitleFiles::languagesMatch(QLocale::languageToCode(language.value<QLocale::Language>()), m_preferredAudioLanguage)) {
        index = int(i);
        break;
      }
    }
  }
  if (index >= 0 && index < tracks.size() && index != m_mediaPlayer->activeAudioTrack()) {
    m_mediaPlayer->setActiveAudioTrack(index);
  }
  emit audioTracksChanged();
}

QUrl CustomMediaPlayer::source() const { return m_source; }

void CustomMediaPlayer::setSource(const QUrl &source) {
  if (source.isEmpty()) {
    // Empty source at startup is not an error, just ignore
    return;
  }
  if (!source.isValid()) {
    emit errorOccurred("Source URL is invalid: " + source.toString());
    return;
  }
  
  checkpoint();

  // Clear video frame from previous source to release memory
  clearMainVideoFrame();
  
  // Reset preview player when source changes
  resetPreviewPlayer();

  resetPlaybackStats();

  clearPendingStep();
  m_pendingAudioTrack = -1;
  m_audioSelectionApplied = false;
  if (source.isLocalFile())
    m_streamLoadTimer->stop();
  else
    m_streamLoadTimer->start();

  const bool changed = m_source != source;
  m_source = source;
  m_resumePositionMs = -1;
  m_pauseWhenLoaded = false;
  m_streamSubtitles.clear();
  m_quality->cancel();
  if (StreamQuality::handles(source)) {
    m_resolvingQuality = true;
    setPlaybackSource({});
    emit mediaStatusChanged(QMediaPlayer::LoadingMedia);
    m_quality->resolve(source, screenHeight());
  } else {
    m_resolvingQuality = false;
    setPlaybackSource({source, {}});
  }
  m_subtitles->setSource(source);
  if (changed)
    emit sourceChanged();
}

void CustomMediaPlayer::onQualityResolved(const StreamQuality::Playback &playback) {
  if (!m_resolvingQuality)
    return;
  m_resolvingQuality = false;
  setPlaybackSource(playback);
}

void CustomMediaPlayer::setPlaybackSource(const StreamQuality::Playback &playback) {
  QBuffer *previous = m_manifest;
  m_manifest = nullptr;
  if (playback.manifest.isEmpty()) {
    m_mediaPlayer->setSource(playback.url);
  } else {
    m_manifest = new QBuffer(this);
    m_manifest->setData(playback.manifest);
    m_manifest->open(QIODevice::ReadOnly);
    m_mediaPlayer->setSourceDevice(m_manifest, playback.url);
  }
  if (previous)
    previous->deleteLater();
}

void CustomMediaPlayer::onQualityFailed(const QString &error) {
  if (!m_resolvingQuality)
    return;
  emit errorOccurred(error);
  stop();
}

void CustomMediaPlayer::selectQuality(int index) {
  if (m_resolvingQuality)
    return;
  switchPlayback(m_quality->select(index));
}

void CustomMediaPlayer::selectAudioFormat(int index) {
  if (m_resolvingQuality)
    return;
  switchPlayback(m_quality->selectAudioFormat(index));
}

void CustomMediaPlayer::switchPlayback(const StreamQuality::Playback &playback) {
  if (playback.url.isEmpty())
    return;

  const QMediaPlayer::PlaybackState state = m_mediaPlayer->playbackState();
  const qint64 resumePosition = m_mediaLoaded && m_mediaPlayer->isSeekable() && m_mediaPlayer->duration() > 0 ? m_mediaPlayer->position() : -1;
  const bool playAfter = m_playWhenLoaded || state == QMediaPlayer::PlayingState;

  resetPreviewPlayer();
  clearPendingStep();
  m_pendingAudioTrack = -1;
  m_audioSelectionApplied = false;
  m_streamLoadTimer->start();
  setPlaybackSource(playback);

  m_resumePositionMs = resumePosition;
  m_playWhenLoaded = playAfter;
  m_pauseWhenLoaded = state == QMediaPlayer::PausedState;
}

QStringList CustomMediaPlayer::qualities() const { return m_quality->labels(); }

int CustomMediaPlayer::activeQuality() const { return m_quality->activeIndex(); }

bool CustomMediaPlayer::autoQuality() const { return m_quality->automatic(); }

QStringList CustomMediaPlayer::audioFormats() const { return m_quality->audioFormats(); }

int CustomMediaPlayer::activeAudioFormat() const { return m_quality->activeAudioFormat(); }

int CustomMediaPlayer::screenHeight() const {
  const QQuickWindow *quickWindow = window();
  const QScreen *screen = quickWindow ? quickWindow->screen() : nullptr;
  return screen ? qRound(screen->size().height() * screen->devicePixelRatio()) : 0;
}

QObject *CustomMediaPlayer::videoOutput() const {
  return m_mediaPlayer->videoOutput();
}

void CustomMediaPlayer::setVideoOutput(QObject *videoOutput) {
  if (m_mediaPlayer->videoOutput() == videoOutput)
    return;
    
  if (m_frameChangedConnection) {
      QObject::disconnect(m_frameChangedConnection);
      m_frameChangedConnection = {};
  }

  m_mediaPlayer->setVideoOutput(videoOutput);
  emit videoOutputChanged();

  m_videoSink = videoOutput ? qobject_cast<QVideoSink *>(videoOutput->property("videoSink").value<QObject *>()) : nullptr;
  updateFrameCounting();
}

void CustomMediaPlayer::updateFrameCounting() {
  const bool shouldCount = m_statsEnabled && m_videoSink;
  if (shouldCount == static_cast<bool>(m_frameChangedConnection))
    return;

  if (shouldCount) {
    m_frameCount = 0;
    m_frameChangedConnection = connect(m_videoSink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        if (frame.isValid()) {
            m_frameCount++;
        }
    });
  } else {
    QObject::disconnect(m_frameChangedConnection);
    m_frameChangedConnection = {};
  }
}

qreal CustomMediaPlayer::fps() const {
    return m_fps;
}

bool CustomMediaPlayer::statsEnabled() const { return m_statsEnabled; }

void CustomMediaPlayer::setStatsEnabled(bool enabled) {
  if (m_statsEnabled == enabled)
    return;
  m_statsEnabled = enabled;
  updateFrameCounting();

  if (m_statsEnabled) {
    updateMediaInfo();
    updateFrameInfo();
    if (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
      resetDroppedFrameWindow();
      m_fpsTimer->start();
    }
  } else {
    m_fpsTimer->stop();
    if (m_fps != 0) {
      m_fps = 0;
      emit fpsChanged();
    }
    updateAudioProbe();
  }
  emit statsEnabledChanged();
}

QVariantMap CustomMediaPlayer::mediaInfo() const { return m_mediaInfo; }

QVariantMap CustomMediaPlayer::frameInfo() const { return m_frameInfo; }

static QString colorSpaceName(QVideoFrameFormat::ColorSpace colorSpace) {
  switch (colorSpace) {
  case QVideoFrameFormat::ColorSpace_BT601: return QStringLiteral("BT.601");
  case QVideoFrameFormat::ColorSpace_BT709: return QStringLiteral("BT.709");
  case QVideoFrameFormat::ColorSpace_AdobeRgb: return QStringLiteral("Adobe RGB");
  case QVideoFrameFormat::ColorSpace_BT2020: return QStringLiteral("BT.2020");
  default: return QStringLiteral("Unknown");
  }
}

static QString colorTransferName(QVideoFrameFormat::ColorTransfer transfer) {
  switch (transfer) {
  case QVideoFrameFormat::ColorTransfer_BT709: return QStringLiteral("BT.709");
  case QVideoFrameFormat::ColorTransfer_BT601: return QStringLiteral("BT.601");
  case QVideoFrameFormat::ColorTransfer_Linear: return QStringLiteral("Linear");
  case QVideoFrameFormat::ColorTransfer_Gamma22: return QStringLiteral("Gamma 2.2");
  case QVideoFrameFormat::ColorTransfer_Gamma28: return QStringLiteral("Gamma 2.8");
  case QVideoFrameFormat::ColorTransfer_ST2084: return QStringLiteral("PQ");
  case QVideoFrameFormat::ColorTransfer_STD_B67: return QStringLiteral("HLG");
  default: return QStringLiteral("Unknown");
  }
}

static QString colorRangeName(QVideoFrameFormat::ColorRange range) {
  switch (range) {
  case QVideoFrameFormat::ColorRange_Video: return QStringLiteral("Limited");
  case QVideoFrameFormat::ColorRange_Full: return QStringLiteral("Full");
  default: return QStringLiteral("Unknown");
  }
}

void CustomMediaPlayer::onFpsTick() {
  const qint64 elapsedNs = m_fpsElapsed.isValid() ? m_fpsElapsed.nsecsElapsed() : 0;
  m_fpsElapsed.start();

  const int frames = m_frameCount;
  m_frameCount = 0;
  const qreal fps = elapsedNs > 0 ? qRound(frames * 1e9 / static_cast<double>(elapsedNs)) : frames;
  if (m_fps != fps) {
    m_fps = fps;
    emit fpsChanged();
  }

  double frameRate = m_mediaInfo.value("frameRate").toDouble();
  if (frameRate <= 0) {
    frameRate = m_frameInfo.value("streamFrameRate").toDouble();
  }

  if (m_skipDropTick) {
    m_skipDropTick = false;
  } else if (frameRate > 0 && elapsedNs > 0) {
    m_expectedFrames += static_cast<double>(elapsedNs) / 1e9 * frameRate * m_mediaPlayer->playbackRate();
    m_shownFrames += frames;
    const double tolerance = qMax(2.0, m_expectedFrames * 0.005);
    const int dropped = qMax(0, static_cast<int>(m_expectedFrames - static_cast<double>(m_shownFrames) - tolerance));
    m_droppedFrames = qMax(m_droppedFrames, m_droppedFramesBase + dropped);
  }

  updateFrameInfo();
}

void CustomMediaPlayer::resetDroppedFrameWindow() {
  if (!m_statsEnabled)
    return;
  m_droppedFramesBase = m_droppedFrames;
  m_frameCount = 0;
  m_expectedFrames = 0;
  m_shownFrames = 0;
  m_skipDropTick = true;
  m_fpsElapsed.start();
}

void CustomMediaPlayer::resetPlaybackStats() {
  if (m_fps != 0) {
    m_fps = 0;
    emit fpsChanged();
  }

  m_droppedFrames = 0;
  m_droppedFramesBase = 0;
  m_probedAudioTrack = -1;
  resetDroppedFrameWindow();

  if (m_mediaPlayer->audioBufferOutput()) {
    m_mediaPlayer->setAudioBufferOutput(nullptr);
  }
  if (!m_mediaInfo.isEmpty()) {
    m_mediaInfo.clear();
    emit mediaInfoChanged();
  }
  if (!m_frameInfo.isEmpty()) {
    m_frameInfo.clear();
    emit frameInfoChanged();
  }
}

void CustomMediaPlayer::updateMediaInfo() {
  if (!m_statsEnabled)
    return;

  const QMediaMetaData global = m_mediaPlayer->metaData();
  const QList<QMediaMetaData> videoTracks = m_mediaPlayer->videoTracks();
  const QList<QMediaMetaData> audioTracks = m_mediaPlayer->audioTracks();
  const int videoIndex = m_mediaPlayer->activeVideoTrack();
  const int audioIndex = m_mediaPlayer->activeAudioTrack();
  const QMediaMetaData videoTrack = videoIndex >= 0 && videoIndex < videoTracks.size() ? videoTracks[videoIndex] : QMediaMetaData();
  const QMediaMetaData audioTrack = audioIndex >= 0 && audioIndex < audioTracks.size() ? audioTracks[audioIndex] : QMediaMetaData();

  auto lookup = [&global](const QMediaMetaData &track, QMediaMetaData::Key key) {
    const QVariant value = track.value(key);
    return value.isValid() ? value : global.value(key);
  };

  QVariantMap info;

  if (!videoTracks.isEmpty()) {
    const QVariant codec = lookup(videoTrack, QMediaMetaData::VideoCodec);
    if (codec.isValid() && codec.value<QMediaFormat::VideoCodec>() != QMediaFormat::VideoCodec::Unspecified) {
      info.insert("videoCodec", QMediaFormat::videoCodecName(codec.value<QMediaFormat::VideoCodec>()));
    }
    if (const int bitRate = lookup(videoTrack, QMediaMetaData::VideoBitRate).toInt(); bitRate > 0) {
      info.insert("videoBitRate", bitRate);
    }
    if (const double frameRate = lookup(videoTrack, QMediaMetaData::VideoFrameRate).toDouble(); frameRate > 0) {
      info.insert("frameRate", frameRate);
    }
    if (const QVariant orientation = lookup(videoTrack, QMediaMetaData::Orientation); orientation.isValid()) {
      info.insert("rotation", orientation.toInt());
    }
    if (const QVariant hdr = lookup(videoTrack, QMediaMetaData::HasHdrContent); hdr.isValid()) {
      info.insert("hdr", hdr.toBool());
    }
  }

  if (!audioTracks.isEmpty()) {
    const QVariant codec = lookup(audioTrack, QMediaMetaData::AudioCodec);
    if (codec.isValid() && codec.value<QMediaFormat::AudioCodec>() != QMediaFormat::AudioCodec::Unspecified) {
      info.insert("audioCodec", QMediaFormat::audioCodecName(codec.value<QMediaFormat::AudioCodec>()));
    }
    if (const int bitRate = lookup(audioTrack, QMediaMetaData::AudioBitRate).toInt(); bitRate > 0) {
      info.insert("audioBitRate", bitRate);
    }
  }

  const QVariant fileFormat = global.value(QMediaMetaData::FileFormat);
  if (fileFormat.isValid() && fileFormat.value<QMediaFormat::FileFormat>() != QMediaFormat::UnspecifiedFormat) {
    info.insert("container", QMediaFormat::fileFormatName(fileFormat.value<QMediaFormat::FileFormat>()));
  }

  const QUrl source = m_source;
  if (source.isLocalFile()) {
    const QFileInfo fileInfo(source.toLocalFile());
    if (fileInfo.exists()) {
      info.insert("fileSize", fileInfo.size());
    }
  }

  if (m_probedAudioTrack == audioIndex) {
    for (const char *key : {"sampleRate", "channels"}) {
      if (m_mediaInfo.contains(key)) {
        info.insert(key, m_mediaInfo.value(key));
      }
    }
  }

  if (info != m_mediaInfo) {
    m_mediaInfo = info;
    emit mediaInfoChanged();
  }

  updateAudioProbe();
}

void CustomMediaPlayer::updateFrameInfo() {
  QVariantMap info;
  info.insert("droppedFrames", m_droppedFrames);

  if (QVideoSink *sink = m_mediaPlayer->videoSink()) {
    const QVideoFrame frame = sink->videoFrame();
    if (frame.isValid()) {
      const QVideoFrameFormat format = frame.surfaceFormat();
      const auto transfer = format.colorTransfer();
      info.insert("decoding", frame.handleType() == QVideoFrame::RhiTextureHandle ? QStringLiteral("Hardware") : QStringLiteral("Software"));
      info.insert("pixelFormat", QVideoFrameFormat::pixelFormatToString(frame.pixelFormat()));
      const bool spaceAssumed = format.colorSpace() == QVideoFrameFormat::ColorSpace_Undefined;
      const bool transferAssumed = transfer == QVideoFrameFormat::ColorTransfer_Unknown;
      const bool rangeAssumed = format.colorRange() == QVideoFrameFormat::ColorRange_Unknown;
      const auto colorSpace = spaceAssumed
          ? (format.frameHeight() > 576 ? QVideoFrameFormat::ColorSpace_BT709 : QVideoFrameFormat::ColorSpace_BT601)
          : format.colorSpace();
      const auto colorRange = rangeAssumed ? QVideoFrameFormat::ColorRange_Video : format.colorRange();
      info.insert("colorSpace", colorSpaceName(colorSpace) + (spaceAssumed ? "*" : ""));
      info.insert("colorTransfer", transferAssumed ? QStringLiteral("SDR*") : colorTransferName(transfer));
      info.insert("colorRange", colorRangeName(colorRange) + (rangeAssumed ? "*" : ""));
      info.insert("colorAssumed", spaceAssumed || transferAssumed || rangeAssumed);
      info.insert("hdr", transfer == QVideoFrameFormat::ColorTransfer_ST2084 || transfer == QVideoFrameFormat::ColorTransfer_STD_B67);
      if (frame.streamFrameRate() > 0) {
        info.insert("streamFrameRate", frame.streamFrameRate());
      }
    }
  }

  if (info != m_frameInfo) {
    m_frameInfo = info;
    emit frameInfoChanged();
  }
}

void CustomMediaPlayer::updateAudioProbe() {
  const bool needsProbe = m_statsEnabled && !m_mediaPlayer->audioTracks().isEmpty() && !m_mediaInfo.contains("sampleRate");
  if (needsProbe == (m_mediaPlayer->audioBufferOutput() != nullptr))
    return;

  if (needsProbe) {
    if (!m_audioBufferOutput) {
      m_audioBufferOutput = new QAudioBufferOutput(this);
      connect(m_audioBufferOutput, &QAudioBufferOutput::audioBufferReceived, this, &CustomMediaPlayer::onAudioBufferReceived);
    }
    m_mediaPlayer->setAudioBufferOutput(m_audioBufferOutput);
  } else {
    m_mediaPlayer->setAudioBufferOutput(nullptr);
  }
}

void CustomMediaPlayer::onAudioBufferReceived(const QAudioBuffer &buffer) {
  const QAudioFormat format = buffer.format();
  if (!m_mediaPlayer->audioBufferOutput() || !format.isValid() || m_mediaInfo.contains("sampleRate"))
    return;

  m_probedAudioTrack = m_mediaPlayer->activeAudioTrack();
  m_mediaInfo.insert("sampleRate", format.sampleRate());
  m_mediaInfo.insert("channels", format.channelCount());
  emit mediaInfoChanged();

  QMetaObject::invokeMethod(this, &CustomMediaPlayer::updateAudioProbe, Qt::QueuedConnection);
}

QAudioOutput *CustomMediaPlayer::audioOutput() const {
  return m_mediaPlayer->audioOutput();
}

void CustomMediaPlayer::setAudioOutput(QAudioOutput *audioOutput) {
  if (m_mediaPlayer->audioOutput() == audioOutput)
    return;
  m_mediaPlayer->setAudioOutput(audioOutput);
  emit audioOutputChanged();
}

QMediaPlayer::PlaybackState CustomMediaPlayer::playbackState() const {
  return m_mediaPlayer->playbackState();
}

QMediaPlayer::MediaStatus CustomMediaPlayer::mediaStatus() const {
  if (m_resolvingQuality)
    return QMediaPlayer::LoadingMedia;
  return m_mediaPlayer->mediaStatus();
}

bool CustomMediaPlayer::hasVideo() const { return m_hasVideo; }

qint64 CustomMediaPlayer::duration() const { return m_mediaPlayer->duration(); }

qint64 CustomMediaPlayer::position() const { return m_mediaPlayer->position(); }

void CustomMediaPlayer::setPosition(qint64 position) {
  clearPendingStep();
  resetDroppedFrameWindow();
  m_mediaPlayer->setPosition(position);
}

bool CustomMediaPlayer::mediaLoaded() const { return m_mediaLoaded; }

void CustomMediaPlayer::play() {
  if (m_source.isEmpty()) {
    return;
  }
  clearPendingStep();
  m_pauseWhenLoaded = false;
  if (m_resolvingQuality || m_mediaPlayer->mediaStatus() < QMediaPlayer::LoadedMedia) {
    m_playWhenLoaded = true;
  } else {
    m_mediaPlayer->play();
  }
}

void CustomMediaPlayer::pause() {
  m_playWhenLoaded = false;
  if (m_resumePositionMs >= 0)
    m_pauseWhenLoaded = true;
  m_mediaPlayer->pause();
}

void CustomMediaPlayer::stop() {
  m_playWhenLoaded = false;
  m_streamLoadTimer->stop();
  checkpoint();
  resetPreviewPlayer();
  m_subtitles->clear();
  m_pendingAudioTrack = -1;
  m_audioSelectionApplied = false;
  m_resolvingQuality = false;
  m_pauseWhenLoaded = false;
  m_resumePositionMs = -1;
  m_streamSubtitles.clear();
  m_quality->cancel();
  m_mediaPlayer->stop();
  setPlaybackSource({});
  if (!m_source.isEmpty()) {
    m_source.clear();
    emit sourceChanged();
  }
  m_mediaPlayer->setPosition(0);
  
  // Explicitly clear the video sink to release video frames
  clearMainVideoFrame();
  resetPlaybackStats();

  // Reset internal state
  if (m_hasVideo) {
    m_hasVideo = false;
    emit hasVideoChanged();
  }
  if (m_mediaLoaded) {
    m_mediaLoaded = false;
    emit mediaLoadedChanged();
  }
  emit videoVisibilityChanged(false);
  emit durationChanged();
  emit positionChanged();
}

void CustomMediaPlayer::updateHasVideo() {
  bool hasVideo = !m_mediaPlayer->videoTracks().isEmpty();
  if (m_hasVideo != hasVideo) {
    m_hasVideo = hasVideo;
    emit hasVideoChanged();
    if (m_mediaLoaded) {
        emit videoVisibilityChanged(m_hasVideo);
    }
  }
}

void CustomMediaPlayer::onStatusChanged(QMediaPlayer::MediaStatus status) {
  updateHasVideo(); // Ensure m_hasVideo is current

  bool loaded = (status >= QMediaPlayer::LoadedMedia &&
                 status != QMediaPlayer::InvalidMedia);
  if (m_mediaLoaded != loaded) {
    m_mediaLoaded = loaded;
    emit mediaLoadedChanged();
  }

  if (status == QMediaPlayer::LoadedMedia) {
    emit videoVisibilityChanged(m_hasVideo);
  } else if (status == QMediaPlayer::NoMedia ||
             status == QMediaPlayer::InvalidMedia) {
    emit videoVisibilityChanged(false);
  }

  if (status == QMediaPlayer::LoadedMedia) {
    applyAudioSelection();
    if (!m_streamSubtitles.isEmpty() && m_mediaPlayer->duration() > 0 && m_mediaPlayer->isSeekable())
      m_subtitles->setStreamTracks(m_streamSubtitles);
  }
  if (status >= QMediaPlayer::LoadedMedia) {
    m_streamLoadTimer->stop();
  }

  if (status == QMediaPlayer::LoadedMedia && m_resumePositionMs >= 0) {
    const qint64 position = std::exchange(m_resumePositionMs, -1);
    const bool playAfter = std::exchange(m_playWhenLoaded, false);
    m_pauseWhenLoaded = false;
    m_mediaPlayer->pause();
    m_mediaPlayer->setPosition(position);
    if (playAfter)
      m_mediaPlayer->play();
  } else if (status == QMediaPlayer::LoadedMedia && m_playWhenLoaded) {
    m_mediaPlayer->play();
    m_playWhenLoaded = false;
  } else if (status == QMediaPlayer::LoadedMedia && std::exchange(m_pauseWhenLoaded, false)) {
    m_mediaPlayer->pause();
  }

  // Connected after the QML-facing forward above, so a repeat mode has already restarted playback.
  if (status == QMediaPlayer::EndOfMedia && !m_statsEnabled &&
      m_mediaPlayer->mediaStatus() == QMediaPlayer::EndOfMedia &&
      m_mediaPlayer->playbackState() == QMediaPlayer::StoppedState) {
    stop();
  }
}

qreal CustomMediaPlayer::playbackRate() const {
  return m_mediaPlayer->playbackRate();
}

void CustomMediaPlayer::setPlaybackRate(qreal rate) {
  if (m_mediaPlayer->playbackRate() == rate)
    return;
  resetDroppedFrameWindow();
  m_mediaPlayer->setPlaybackRate(rate);
  emit playbackRateChanged();
}

void CustomMediaPlayer::captureFrame() {
  QVideoSink *sink = m_mediaPlayer->videoSink();
  if (!sink) {
    emit frameCaptured(false, "No video sink available.");
    return;
  }

  QVideoFrame frame = sink->videoFrame();
  if (!frame.isValid()) {
    emit frameCaptured(false, "Invalid video frame.");
    return;
  }

  QImage image = frame.toImage();
  if (image.isNull()) {
    emit frameCaptured(false, "Failed to convert frame to image.");
    return;
  }

  QString videoName = m_source.fileName();
  videoName = videoName.left(videoName.lastIndexOf('.'));

  qint64 pos = m_mediaPlayer->position();
  QString timeOfFrame = QDateTime::fromMSecsSinceEpoch(pos).toUTC().toString("hh-mm-ss-zzz");

  QString systemTime = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");

  QString filename = QString("%1_%2_%3.jpeg").arg(videoName, timeOfFrame, systemTime);

  QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
  if (picturesPath.isEmpty()) {
    emit frameCaptured(false, "Could not determine pictures location.");
    return;
  }

  QDir dir(picturesPath);
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  QString fullPath = dir.filePath(filename);

  if (image.save(fullPath, "JPEG")) {
    emit frameCaptured(true, fullPath);
  } else {
    emit frameCaptured(false, "Failed to save image.");
  }
}

void CustomMediaPlayer::requestPreviewAt(qint64 position) {
  if (!m_hasVideo || !m_source.isLocalFile() || position < 0) {
    return;
  }

  // Initialize preview player if needed
  if (!m_previewPlayer) {
    m_previewPlayer = new QMediaPlayer(this);
    m_previewSink = new QVideoSink(this);
    m_previewPlayer->setVideoSink(m_previewSink);
    // No audio output - preview is silent
    m_previewPlayer->setAudioOutput(nullptr);

    connect(m_previewPlayer, &QMediaPlayer::mediaStatusChanged,
            this, &CustomMediaPlayer::onPreviewPlayerStatusChanged);

    connect(m_previewSink, &QVideoSink::videoFrameChanged,
            this, &CustomMediaPlayer::onPreviewFrameChanged);
  }

  // Store the position we want to preview
  m_pendingPreviewPosition = position;

  // Load the same source if different
  if (m_previewPlayer->source() != m_mediaPlayer->source()) {
    m_previewPlayer->setSource(m_mediaPlayer->source());
  } else if (m_previewPlayer->mediaStatus() >= QMediaPlayer::LoadedMedia) {
    startPreviewCapture(position);
  }
}

void CustomMediaPlayer::startPreviewCapture(qint64 position) {
  if (!m_previewPlayer || !m_previewSink) {
    return;
  }

  if (position < 0) {
    return;
  }

  m_waitingForPreview = true;
  m_previewPlayer->setPosition(position);
  m_previewPlayer->pause();
}

void CustomMediaPlayer::onPreviewFrameChanged() {
    if (m_waitingForPreview && m_pendingPreviewPosition >= 0) {
        m_waitingForPreview = false;
        capturePreviewFrame();
    }
}

void CustomMediaPlayer::onPreviewPlayerStatusChanged(QMediaPlayer::MediaStatus status) {
  if (status == QMediaPlayer::LoadedMedia && m_pendingPreviewPosition >= 0) {
    startPreviewCapture(m_pendingPreviewPosition);
  }
}

// Removed timer logic
void CustomMediaPlayer::onMediaPlayerError(QMediaPlayer::Error error,
                                           const QString &errorString) {
  if (error != QMediaPlayer::NoError) {
    m_streamLoadTimer->stop();
    qWarning() << "MediaPlayer Error:" << error << errorString;
    emit errorOccurred(errorString);
  }
}

void CustomMediaPlayer::capturePreviewFrame() {
  if (!m_previewSink) {
    return;
  }

  QVideoFrame frame = m_previewSink->videoFrame();
  if (!frame.isValid()) {
    return;
  }

  QImage image = frame.toImage();
  if (image.isNull()) {
    return;
  }

  // Scale down for preview (max 160x90 for 16:9)
  QImage scaled = image.scaled(160, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  QString imageId = QString("preview_%1").arg(QDateTime::currentMSecsSinceEpoch());

  if (auto provider = PreviewImageProvider::instance()) {
      provider->storeImage(imageId, scaled);
      QString imageUrl = "image://preview/" + imageId;
      emit previewReady(m_pendingPreviewPosition, imageUrl);
  }
}

void CustomMediaPlayer::resetPreviewPlayer() {
  m_pendingPreviewPosition = -1;
  m_waitingForPreview = false;
  
  // Properly cleanup preview player and sink to release memory
  if (m_previewPlayer) {
    m_previewPlayer->stop();
    m_previewPlayer->setSource(QUrl());
    // Disassociate sink from player before deletion
    m_previewPlayer->setVideoSink(nullptr);
    
    // Delete player and sink to release video frames and memory
    delete m_previewPlayer;
    m_previewPlayer = nullptr;
    delete m_previewSink;
    m_previewSink = nullptr;
  }
  
  // Clear all cached preview images from the provider
  if (auto provider = PreviewImageProvider::instance()) {
    provider->clearImages();
  }
}

void CustomMediaPlayer::clearMainVideoFrame() {
  // Clear video frame from main player's sink to release memory
  if (QVideoSink *sink = m_mediaPlayer->videoSink()) {
    sink->setVideoFrame(QVideoFrame());
  }
}

