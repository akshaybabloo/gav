#include "custommediaplayer.h"

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

