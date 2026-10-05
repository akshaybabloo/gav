#include "mediasession.h"

#include <QCoreApplication>
#include <QPointer>

#include <spdlog/spdlog.h>

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <MediaPlayer/MediaPlayer.h>

#include <atomic>
#include <functional>
#include <memory>

namespace {

class MacBackend : public MediaSessionBackend {
public:
    explicit MacBackend(MediaSession *session) : m_session(session) {}

    ~MacBackend() override {
        MPRemoteCommandCenter *center = [MPRemoteCommandCenter sharedCommandCenter];
        for (MPRemoteCommand *command in commands(center)) {
            [command removeTarget:nil];
        }
        [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = nil;
    }

    bool initialise(QWindow *) override {
        MPRemoteCommandCenter *center = [MPRemoteCommandCenter sharedCommandCenter];
        if (!center) {
            return false;
        }

        addHandler(center.playCommand, [](MediaSession *session, double) { session->requestPlay(); });
        addHandler(center.pauseCommand, [](MediaSession *session, double) { session->requestPause(); });
        addHandler(center.togglePlayPauseCommand, [](MediaSession *session, double) { session->requestPlayPause(); });
        addHandler(center.stopCommand, [](MediaSession *session, double) { session->requestStop(); });
        addHandler(center.nextTrackCommand, [](MediaSession *session, double) { session->requestNext(); });
        addHandler(center.previousTrackCommand, [](MediaSession *session, double) { session->requestPrevious(); });
        addHandler(center.changePlaybackPositionCommand,
                   [](MediaSession *session, double seconds) { session->requestSeek(static_cast<qint64>(seconds * 1000.0)); });
        return true;
    }

    void stateChanged(const MediaSessionState &state) override {
        m_hasMedia->store(state.hasMedia);
        MPRemoteCommandCenter *center = [MPRemoteCommandCenter sharedCommandCenter];
        center.playCommand.enabled = state.hasMedia;
        center.pauseCommand.enabled = state.hasMedia;
        center.togglePlayPauseCommand.enabled = state.hasMedia;
        center.stopCommand.enabled = state.hasMedia;
        center.nextTrackCommand.enabled = state.hasMedia && state.canGoNext;
        center.previousTrackCommand.enabled = state.hasMedia && state.canGoPrevious;
        center.changePlaybackPositionCommand.enabled = state.hasMedia && state.canSeek;

        if (state.artworkUrl != m_artworkUrl) {
            m_artworkUrl = state.artworkUrl;
            m_artwork = nil;
            if (state.artworkUrl.isLocalFile()) {
                NSImage *image = [[NSImage alloc] initWithContentsOfFile:state.artworkUrl.toLocalFile().toNSString()];
                if (image) {
                    m_artwork = [[MPMediaItemArtwork alloc] initWithBoundsSize:image.size
                                                                requestHandler:^NSImage *(CGSize) {
                                                                    return image;
                                                                }];
                }
            }
        }
        publish(state);
    }

    void positionChanged(const MediaSessionState &state, bool seeked) override {
        if (seeked) {
            publish(state);
        }
    }

private:
    using Action = std::function<void(MediaSession *, double)>;

    static NSArray<MPRemoteCommand *> *commands(MPRemoteCommandCenter *center) {
        return @[
            center.playCommand, center.pauseCommand, center.togglePlayPauseCommand, center.stopCommand, center.nextTrackCommand,
            center.previousTrackCommand, center.changePlaybackPositionCommand
        ];
    }

    void addHandler(MPRemoteCommand *command, Action action) {
        const QPointer<MediaSession> session = m_session;
        const std::shared_ptr<std::atomic<bool>> hasMedia = m_hasMedia;
        [command addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
            if (!hasMedia->load()) {
                return MPRemoteCommandHandlerStatusCommandFailed;
            }
            double seconds = 0;
            if ([event isKindOfClass:[MPChangePlaybackPositionCommandEvent class]]) {
                seconds = static_cast<MPChangePlaybackPositionCommandEvent *>(event).positionTime;
            }
            QMetaObject::invokeMethod(
                qApp,
                [session, action, seconds] {
                    if (session) {
                        action(session, seconds);
                    }
                },
                Qt::QueuedConnection);
            return MPRemoteCommandHandlerStatusSuccess;
        }];
    }

    void publish(const MediaSessionState &state) {
        MPNowPlayingInfoCenter *center = [MPNowPlayingInfoCenter defaultCenter];
        if (!state.hasMedia) {
            center.nowPlayingInfo = nil;
            center.playbackState = MPNowPlayingPlaybackStateStopped;
            return;
        }

        const bool playing = state.playbackStatus == QLatin1String("Playing");
        NSMutableDictionary *info = [NSMutableDictionary dictionary];
        info[MPMediaItemPropertyTitle] = state.title.toNSString();
        if (!state.artist.isEmpty()) {
            info[MPMediaItemPropertyArtist] = state.artist.toNSString();
        }
        if (!state.album.isEmpty()) {
            info[MPMediaItemPropertyAlbumTitle] = state.album.toNSString();
        }
        if (state.durationMs > 0) {
            info[MPMediaItemPropertyPlaybackDuration] = @(state.durationMs / 1000.0);
        }
        info[MPNowPlayingInfoPropertyElapsedPlaybackTime] = @(state.positionMs / 1000.0);
        info[MPNowPlayingInfoPropertyPlaybackRate] = @(playing ? state.rate : 0.0);
        info[MPNowPlayingInfoPropertyMediaType] = @(state.hasVideo ? MPNowPlayingInfoMediaTypeVideo : MPNowPlayingInfoMediaTypeAudio);
        if (m_artwork) {
            info[MPMediaItemPropertyArtwork] = m_artwork;
        }
        center.nowPlayingInfo = info;
        center.playbackState = playing ? MPNowPlayingPlaybackStatePlaying
                                       : (state.playbackStatus == QLatin1String("Paused") ? MPNowPlayingPlaybackStatePaused
                                                                                          : MPNowPlayingPlaybackStateStopped);
    }

    QPointer<MediaSession> m_session;
    std::shared_ptr<std::atomic<bool>> m_hasMedia = std::make_shared<std::atomic<bool>>(false);
    QUrl m_artworkUrl;
    MPMediaItemArtwork *m_artwork = nil;
};

}

std::unique_ptr<MediaSessionBackend> createPlatformMediaSessionBackend(MediaSession *session) { return std::make_unique<MacBackend>(session); }
