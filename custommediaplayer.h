#ifndef CUSTOMMEDIAPLAYER_H
#define CUSTOMMEDIAPLAYER_H

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QMediaMetaData>
#include <QQuickItem>
#include <QTimer>
#include <QUrl>
#include <QVideoSink>

class CustomMediaPlayer : public QQuickItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
  Q_PROPERTY(QObject *videoOutput READ videoOutput WRITE setVideoOutput NOTIFY videoOutputChanged)
  Q_PROPERTY(QAudioOutput *audioOutput READ audioOutput WRITE setAudioOutput NOTIFY audioOutputChanged)
  Q_PROPERTY(QMediaPlayer::PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)
  Q_PROPERTY(QMediaPlayer::MediaStatus mediaStatus READ mediaStatus NOTIFY mediaStatusChanged)
  Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY hasVideoChanged)
  Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
  Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)
  Q_PROPERTY(bool mediaLoaded READ mediaLoaded NOTIFY mediaLoadedChanged)
  Q_PROPERTY(qreal playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)
  Q_PROPERTY(qreal fps READ fps NOTIFY fpsChanged)

public:
  CustomMediaPlayer();

  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void stop();
  Q_INVOKABLE void captureFrame();
  Q_INVOKABLE void requestPreviewAt(qint64 position);

  QUrl source() const;
  void setSource(const QUrl &source);

  qreal playbackRate() const;
  void setPlaybackRate(qreal rate);

  QObject *videoOutput() const;
  void setVideoOutput(QObject *videoOutput);

  QAudioOutput *audioOutput() const;
  void setAudioOutput(QAudioOutput *audioOutput);

  QMediaPlayer::PlaybackState playbackState() const;
  QMediaPlayer::MediaStatus mediaStatus() const;

  bool hasVideo() const;

  qint64 duration() const;

  qint64 position() const;
  void setPosition(qint64 position);

  bool mediaLoaded() const;

  qreal fps() const;

signals:
  void sourceChanged();
  void videoOutputChanged();
  void audioOutputChanged();
  void playbackStateChanged(QMediaPlayer::PlaybackState state);
  void mediaStatusChanged(QMediaPlayer::MediaStatus status);
  void hasVideoChanged();
  void errorOccurred(QString errorString);
  void durationChanged();
  void positionChanged();
  void mediaLoadedChanged();
  void playbackRateChanged();
  void fpsChanged();
  void videoVisibilityChanged(bool visible);
  void frameCaptured(bool success, const QString &path);
  void previewReady(qint64 position, const QString &imageDataUrl);

private slots:
  void onPreviewPlayerStatusChanged(QMediaPlayer::MediaStatus status);
  void onPreviewFrameChanged();
  void onMediaPlayerError(QMediaPlayer::Error error, const QString &errorString);
  void updateHasVideo();
  void onStatusChanged(QMediaPlayer::MediaStatus status);

private:
  void capturePreviewFrame();
  void resetPreviewPlayer();
  void startPreviewCapture(qint64 position);
  void clearMainVideoFrame();

  QMediaPlayer *m_mediaPlayer;
  QMediaPlayer *m_previewPlayer = nullptr;
  QVideoSink *m_previewSink = nullptr;
  qint64 m_pendingPreviewPosition = -1;
  bool m_hasVideo = false;
  bool m_playWhenLoaded = false;
  bool m_mediaLoaded = false;
  bool m_waitingForPreview = false;
  qreal m_fps = 0;
  int m_frameCount = 0;
  QTimer *m_fpsTimer = nullptr;
  QMetaObject::Connection m_frameChangedConnection;
};

#endif // CUSTOMMEDIAPLAYER_H
