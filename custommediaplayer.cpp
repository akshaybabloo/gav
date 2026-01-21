#include "custommediaplayer.h"
#include <QVideoSink>
#include <QVideoFrame>
#include <QImage>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QBuffer>
#include <QTimer>

CustomMediaPlayer::CustomMediaPlayer() {
  m_mediaPlayer = new QMediaPlayer(this);

  // Forward signals from QMediaPlayer
  connect(m_mediaPlayer, &QMediaPlayer::sourceChanged, this,
          &CustomMediaPlayer::sourceChanged);
  connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this,
          &CustomMediaPlayer::playbackStateChanged);
  connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this,
          &CustomMediaPlayer::mediaStatusChanged);
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
  m_mediaPlayer->setSource(source);
}

QObject *CustomMediaPlayer::videoOutput() const {
  return m_mediaPlayer->videoOutput();
}

void CustomMediaPlayer::setVideoOutput(QObject *videoOutput) {
  if (m_mediaPlayer->videoOutput() == videoOutput)
    return;
  m_mediaPlayer->setVideoOutput(videoOutput);
  emit videoOutputChanged();
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
  m_mediaPlayer->setPosition(position);
}

bool CustomMediaPlayer::mediaLoaded() const { return m_mediaLoaded; }

void CustomMediaPlayer::play() {
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
  m_mediaPlayer->stop();
  m_mediaPlayer->setSource(QUrl());
  m_mediaPlayer->setPosition(0);

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

void CustomMediaPlayer::onMediaPlayerError(QMediaPlayer::Error error,
                                           const QString &errorString) {
  if (error != QMediaPlayer::NoError) {
    qWarning() << "MediaPlayer Error:" << error << errorString;
    emit errorOccurred(errorString);
  }
}

void CustomMediaPlayer::updateHasVideo() {
  bool hasVideo = !m_mediaPlayer->videoTracks().isEmpty();
  if (m_hasVideo != hasVideo) {
    m_hasVideo = hasVideo;
    emit hasVideoChanged();
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
}

qreal CustomMediaPlayer::playbackRate() const {
  return m_mediaPlayer->playbackRate();
}

void CustomMediaPlayer::setPlaybackRate(qreal rate) {
  if (m_mediaPlayer->playbackRate() == rate)
    return;
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
  if (!m_hasVideo || m_mediaPlayer->source().isEmpty()) {
    return;
  }

  // Initialize preview player if needed
  if (!m_previewPlayer) {
    m_previewPlayer = new QMediaPlayer(this);
    m_previewSink = new QVideoSink(this);
    m_previewPlayer->setVideoSink(m_previewSink);

    connect(m_previewPlayer, &QMediaPlayer::mediaStatusChanged,
            this, &CustomMediaPlayer::onPreviewPlayerStatusChanged);
  }

  // Store the position we want to preview
  m_pendingPreviewPosition = position;

  // Load the same source if different
  if (m_previewPlayer->source() != m_mediaPlayer->source()) {
    m_previewPlayer->setSource(m_mediaPlayer->source());
  } else if (m_previewPlayer->mediaStatus() >= QMediaPlayer::LoadedMedia) {
    // Source already loaded, seek directly
    m_previewPlayer->setPosition(position);
    // Capture after a short delay to allow frame to render
    QTimer::singleShot(100, this, [this, position]() {
      capturePreviewFrame(position);
    });
  }
}

void CustomMediaPlayer::onPreviewPlayerStatusChanged(QMediaPlayer::MediaStatus status) {
  if (status == QMediaPlayer::LoadedMedia && m_pendingPreviewPosition >= 0) {
    m_previewPlayer->setPosition(m_pendingPreviewPosition);
    // Capture after a short delay to allow frame to render
    qint64 pos = m_pendingPreviewPosition;
    QTimer::singleShot(100, this, [this, pos]() {
      capturePreviewFrame(pos);
    });
  }
}

void CustomMediaPlayer::capturePreviewFrame(qint64 position) {
  if (!m_previewSink) {
    emit previewReady(position, QString());
    return;
  }

  QVideoFrame frame = m_previewSink->videoFrame();
  if (!frame.isValid()) {
    emit previewReady(position, QString());
    return;
  }

  QImage image = frame.toImage();
  if (image.isNull()) {
    emit previewReady(position, QString());
    return;
  }

  // Scale down for preview (max 160x90 for 16:9)
  QImage scaled = image.scaled(160, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  // Convert to base64 data URL
  QByteArray byteArray;
  QBuffer buffer(&byteArray);
  buffer.open(QIODevice::WriteOnly);
  scaled.save(&buffer, "JPEG", 70);

  QString dataUrl = "data:image/jpeg;base64," + QString::fromLatin1(byteArray.toBase64());
  emit previewReady(position, dataUrl);
}

