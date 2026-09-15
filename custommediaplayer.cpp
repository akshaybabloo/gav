#include "custommediaplayer.h"
#include "previewimageprovider.h"
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
#include <QMediaFormat>
#include <QVideoFrameFormat>

CustomMediaPlayer::CustomMediaPlayer() {
  m_mediaPlayer = new QMediaPlayer(this);

  // Forward signals from QMediaPlayer
  connect(m_mediaPlayer, &QMediaPlayer::sourceChanged, this,
          &CustomMediaPlayer::sourceChanged);
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
}

QUrl CustomMediaPlayer::source() const { return m_mediaPlayer->source(); }

void CustomMediaPlayer::setSource(const QUrl &source) {
  if (source.isEmpty()) {
    // Empty source at startup is not an error, just ignore
    return;
  }
  if (!source.isValid()) {
    emit errorOccurred("Source URL is invalid: " + source.toString());
    return;
  }
  
  // Clear video frame from previous source to release memory
  clearMainVideoFrame();
  
  // Reset preview player when source changes
  resetPreviewPlayer();

  resetPlaybackStats();

  m_mediaPlayer->setSource(source);
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

  const QUrl source = m_mediaPlayer->source();
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
  return m_mediaPlayer->mediaStatus();
}

bool CustomMediaPlayer::hasVideo() const { return m_hasVideo; }

qint64 CustomMediaPlayer::duration() const { return m_mediaPlayer->duration(); }

qint64 CustomMediaPlayer::position() const { return m_mediaPlayer->position(); }

void CustomMediaPlayer::setPosition(qint64 position) {
  resetDroppedFrameWindow();
  m_mediaPlayer->setPosition(position);
}

bool CustomMediaPlayer::mediaLoaded() const { return m_mediaLoaded; }

void CustomMediaPlayer::play() {
  if (m_mediaPlayer->source().isEmpty()) {
    return;
  }
  if (m_mediaPlayer->mediaStatus() < QMediaPlayer::LoadedMedia) {
    m_playWhenLoaded = true;
  } else {
    m_mediaPlayer->play();
  }
}

void CustomMediaPlayer::pause() {
  m_playWhenLoaded = false;
  m_mediaPlayer->pause();
}

void CustomMediaPlayer::stop() {
  m_playWhenLoaded = false;
  resetPreviewPlayer();
  m_mediaPlayer->stop();
  m_mediaPlayer->setSource(QUrl());
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

  if (status == QMediaPlayer::LoadedMedia && m_playWhenLoaded) {
    m_mediaPlayer->play();
    m_playWhenLoaded = false;
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

  QString videoName = m_mediaPlayer->source().fileName();
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
  if (!m_hasVideo || m_mediaPlayer->source().isEmpty() || position < 0) {
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

