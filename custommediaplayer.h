#ifndef CUSTOMMEDIAPLAYER_H
#define CUSTOMMEDIAPLAYER_H

#include <QAudioBuffer>
#include <QAudioBufferOutput>
#include <QAudioOutput>
#include <QElapsedTimer>
#include <QImage>
#include <QMediaPlayer>
#include <QMediaMetaData>
#include <QPointer>
#include <QQuickItem>
#include <QTimer>
#include <QUrl>
#include <QVideoSink>

#include "streamquality.h"
#include "subtitlecontroller.h"

class QBuffer;

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
  Q_PROPERTY(bool statsEnabled READ statsEnabled WRITE setStatsEnabled NOTIFY statsEnabledChanged)
  Q_PROPERTY(QVariantMap mediaInfo READ mediaInfo NOTIFY mediaInfoChanged)
  Q_PROPERTY(QVariantMap frameInfo READ frameInfo NOTIFY frameInfoChanged)
  Q_PROPERTY(SubtitleController *subtitles READ subtitles CONSTANT)
  Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY audioTracksChanged)
  Q_PROPERTY(int activeAudioTrack READ activeAudioTrack NOTIFY audioTracksChanged)
  Q_PROPERTY(QString activeAudioTrackName READ activeAudioTrackName NOTIFY audioTracksChanged)
  Q_PROPERTY(QString preferredAudioLanguage READ preferredAudioLanguage WRITE setPreferredAudioLanguage NOTIFY preferredAudioLanguageChanged)
  Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
  Q_PROPERTY(bool seekable READ seekable NOTIFY liveChanged)
  Q_PROPERTY(bool isLive READ isLive NOTIFY liveChanged)
  Q_PROPERTY(bool buffering READ buffering NOTIFY bufferingChanged)
  Q_PROPERTY(QStringList qualities READ qualities NOTIFY qualitiesChanged)
  Q_PROPERTY(int activeQuality READ activeQuality NOTIFY qualitiesChanged)
  Q_PROPERTY(bool autoQuality READ autoQuality NOTIFY qualitiesChanged)
  Q_PROPERTY(QStringList audioFormats READ audioFormats NOTIFY qualitiesChanged)
  Q_PROPERTY(int activeAudioFormat READ activeAudioFormat NOTIFY qualitiesChanged)
  Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY nowPlayingChanged)
  Q_PROPERTY(QString mediaArtist READ mediaArtist NOTIFY nowPlayingChanged)
  Q_PROPERTY(QString mediaAlbum READ mediaAlbum NOTIFY nowPlayingChanged)
  Q_PROPERTY(QImage coverArt READ coverArt NOTIFY nowPlayingChanged)

public:
  CustomMediaPlayer();

  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void stop();
  Q_INVOKABLE void captureFrame();
  Q_INVOKABLE void requestPreviewAt(qint64 position);
  Q_INVOKABLE void selectAudioTrack(int index);
  Q_INVOKABLE void checkpoint();
  Q_INVOKABLE bool stepFrame(int direction);
  Q_INVOKABLE QString nextChapter();
  Q_INVOKABLE QString previousChapter();
  Q_INVOKABLE QString cycleAudioTrack();
  Q_INVOKABLE void selectQuality(int index);
  Q_INVOKABLE void selectAudioFormat(int index);

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

  bool statsEnabled() const;
  void setStatsEnabled(bool enabled);

  QVariantMap mediaInfo() const;
  QVariantMap frameInfo() const;

  SubtitleController *subtitles() const;
  QVariantList audioTracks() const;
  int activeAudioTrack() const;
  QString activeAudioTrackName() const;
  QString preferredAudioLanguage() const;
  void setPreferredAudioLanguage(const QString &language);
  QVariantList chapters() const;
  bool seekable() const;
  bool isLive() const;
  bool buffering() const;
  QStringList qualities() const;
  int activeQuality() const;
  bool autoQuality() const;
  QStringList audioFormats() const;
  int activeAudioFormat() const;
  QString mediaTitle() const;
  QString mediaArtist() const;
  QString mediaAlbum() const;
  QImage coverArt() const;

  static constexpr int streamNetworkTimeoutMs = 10000;
  static constexpr int streamLoadTimeoutMs = 120000;
  static constexpr int stallPollIntervalMs = 250;
  static constexpr int stallThresholdMs = 750;

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
  void statsEnabledChanged();
  void mediaInfoChanged();
  void frameInfoChanged();
  void videoVisibilityChanged(bool visible);
  void frameCaptured(bool success, const QString &path);
  void previewReady(qint64 position, const QString &imageDataUrl);
  void audioTracksChanged();
  void positionCheckpoint(const QUrl &source, qint64 position, qint64 duration);
  void preferredAudioLanguageChanged();
  void chaptersChanged();
  void liveChanged();
  void bufferingChanged();
  void qualitiesChanged();
  void nowPlayingChanged();

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
  void onFpsTick();
  void updateFrameCounting();
  void resetDroppedFrameWindow();
  void resetPlaybackStats();
  void updateMediaInfo();
  void updateFrameInfo();
  void updateAudioProbe();
  void onAudioBufferReceived(const QAudioBuffer &buffer);
  void applyAudioSelection();
  QString jumpChapter(int direction);
  double frameDurationUs() const;
  void clearPendingStep();
  void onStreamLoadTimeout();
  void checkForStall();
  void setBuffering(bool buffering);
  void onQualityResolved(const StreamQuality::Playback &playback);
  void setPlaybackSource(const StreamQuality::Playback &playback);
  void switchPlayback(const StreamQuality::Playback &playback);
  void onQualityFailed(const QString &error);
  int screenHeight() const;
  QString audioTrackName(int index) const;

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
  QPointer<QVideoSink> m_videoSink;
  QElapsedTimer m_fpsElapsed;

  bool m_statsEnabled = false;
  QVariantMap m_mediaInfo;
  QVariantMap m_frameInfo;
  QAudioBufferOutput *m_audioBufferOutput = nullptr;

  double m_expectedFrames = 0;
  qint64 m_shownFrames = 0;
  int m_droppedFrames = 0;
  int m_droppedFramesBase = 0;
  bool m_skipDropTick = true;
  int m_probedAudioTrack = -1;

  SubtitleController *m_subtitles = nullptr;
  QString m_preferredAudioLanguage;
  int m_pendingAudioTrack = -1;
  bool m_audioSelectionApplied = false;

  qint64 m_stepBaseUs = -1;
  QMetaObject::Connection m_stepConnection;
  QTimer *m_stepTimer = nullptr;
  QTimer *m_streamLoadTimer = nullptr;
  QTimer *m_stallTimer = nullptr;
  QElapsedTimer m_stallClock;
  qint64 m_stallPosition = -1;
  bool m_buffering = false;

  QUrl m_source;
  StreamQuality *m_quality = nullptr;
  bool m_resolvingQuality = false;
  bool m_pauseWhenLoaded = false;
  qint64 m_resumePositionMs = -1;
  QList<HlsSubtitle> m_streamSubtitles;
  QBuffer *m_manifest = nullptr;
};

#endif // CUSTOMMEDIAPLAYER_H
