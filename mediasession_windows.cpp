#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "mediasession.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QPointer>
#include <QWindow>

#include <spdlog/spdlog.h>

#include <windows.h>

#include <systemmediatransportcontrolsinterop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>

#include <atomic>
#include <chrono>
#include <memory>

namespace {

using namespace winrt::Windows::Media;
using winrt::Windows::Storage::StorageFile;
using winrt::Windows::Storage::Streams::RandomAccessStreamReference;

constexpr qint64 timelineIntervalMs = 1000;

winrt::hstring toHString(const QString &text) { return winrt::hstring(reinterpret_cast<const wchar_t *>(text.utf16()), static_cast<uint32_t>(text.size())); }

class WindowsBackend : public MediaSessionBackend {
public:
    explicit WindowsBackend(MediaSession *session) : m_session(session) {}

    ~WindowsBackend() override {
        if (!m_controls) {
            return;
        }
        try {
            m_controls.ButtonPressed(m_buttonToken);
            m_controls.PlaybackPositionChangeRequested(m_positionToken);
            m_controls.IsEnabled(false);
        } catch (const winrt::hresult_error &) {
        }
    }

    bool initialise(QWindow *window) override {
        if (!window) {
            return false;
        }
        try {
            const auto interop = winrt::get_activation_factory<SystemMediaTransportControls, ISystemMediaTransportControlsInterop>();
            winrt::check_hresult(interop->GetForWindow(reinterpret_cast<HWND>(window->winId()), winrt::guid_of<SystemMediaTransportControls>(),
                                                       winrt::put_abi(m_controls)));
            m_controls.IsEnabled(true);
            m_controls.IsPlayEnabled(true);
            m_controls.IsPauseEnabled(true);
            m_controls.IsStopEnabled(true);

            const QPointer<MediaSession> session = m_session;
            m_buttonToken = m_controls.ButtonPressed([session](const SystemMediaTransportControls &, const SystemMediaTransportControlsButtonPressedEventArgs &args) {
                const SystemMediaTransportControlsButton button = args.Button();
                QMetaObject::invokeMethod(
                    qApp,
                    [session, button] {
                        if (!session) {
                            return;
                        }
                        switch (button) {
                        case SystemMediaTransportControlsButton::Play:
                            session->requestPlay();
                            break;
                        case SystemMediaTransportControlsButton::Pause:
                            session->requestPause();
                            break;
                        case SystemMediaTransportControlsButton::Stop:
                            session->requestStop();
                            break;
                        case SystemMediaTransportControlsButton::Next:
                            session->requestNext();
                            break;
                        case SystemMediaTransportControlsButton::Previous:
                            session->requestPrevious();
                            break;
                        default:
                            break;
                        }
                    },
                    Qt::QueuedConnection);
            });
            m_positionToken = m_controls.PlaybackPositionChangeRequested(
                [session](const SystemMediaTransportControls &, const PlaybackPositionChangeRequestedEventArgs &args) {
                    const qint64 positionMs = std::chrono::duration_cast<std::chrono::milliseconds>(args.RequestedPlaybackPosition()).count();
                    QMetaObject::invokeMethod(
                        qApp,
                        [session, positionMs] {
                            if (session) {
                                session->requestSeek(positionMs);
                            }
                        },
                        Qt::QueuedConnection);
                });
        } catch (const winrt::hresult_error &error) {
            spdlog::warn("Could not set up Windows media controls: {}", winrt::to_string(error.message()));
            m_controls = nullptr;
            return false;
        }
        return true;
    }

    void stateChanged(const MediaSessionState &state) override {
        if (!m_controls) {
            return;
        }
        try {
            MediaPlaybackStatus status = MediaPlaybackStatus::Closed;
            if (state.hasMedia) {
                status = state.playbackStatus == QLatin1String("Playing")  ? MediaPlaybackStatus::Playing
                         : state.playbackStatus == QLatin1String("Paused") ? MediaPlaybackStatus::Paused
                                                                           : MediaPlaybackStatus::Stopped;
            }
            m_controls.PlaybackStatus(status);
            m_controls.IsNextEnabled(state.hasMedia && state.canGoNext);
            m_controls.IsPreviousEnabled(state.hasMedia && state.canGoPrevious);

            const bool metadataChanged = state.title != m_last.title || state.artist != m_last.artist || state.album != m_last.album ||
                                         state.hasVideo != m_last.hasVideo || state.hasMedia != m_last.hasMedia;
            const bool artworkChanged = state.artworkUrl != m_last.artworkUrl;
            if (metadataChanged || artworkChanged) {
                SystemMediaTransportControlsDisplayUpdater updater = m_controls.DisplayUpdater();
                if (!state.hasMedia) {
                    updater.ClearAll();
                } else if (state.hasVideo) {
                    updater.Type(MediaPlaybackType::Video);
                    updater.VideoProperties().Title(toHString(state.title));
                    updater.VideoProperties().Subtitle(toHString(state.artist));
                } else {
                    updater.Type(MediaPlaybackType::Music);
                    updater.MusicProperties().Title(toHString(state.title));
                    updater.MusicProperties().Artist(toHString(state.artist));
                    updater.MusicProperties().AlbumTitle(toHString(state.album));
                }
                updater.Update();
            }
            if (artworkChanged && state.hasMedia) {
                updateThumbnail(state.artworkUrl);
            }
            if (state.durationMs != m_last.durationMs || state.canSeek != m_last.canSeek || state.hasMedia != m_last.hasMedia) {
                updateTimeline(state);
            }
        } catch (const winrt::hresult_error &error) {
            spdlog::debug("Windows media controls update failed: {}", winrt::to_string(error.message()));
        }
        m_last = state;
    }

    void positionChanged(const MediaSessionState &state, bool seeked) override {
        m_last.positionMs = state.positionMs;
        if (!m_controls || !state.hasMedia) {
            return;
        }
        if (!seeked && m_timelineClock.isValid() && m_timelineClock.elapsed() < timelineIntervalMs) {
            return;
        }
        try {
            updateTimeline(state);
        } catch (const winrt::hresult_error &) {
        }
    }

private:
    void updateTimeline(const MediaSessionState &state) {
        m_timelineClock.start();
        if (state.durationMs <= 0) {
            return;
        }
        using std::chrono::milliseconds;
        SystemMediaTransportControlsTimelineProperties timeline;
        timeline.StartTime(milliseconds(0));
        timeline.MinSeekTime(milliseconds(0));
        timeline.EndTime(milliseconds(state.durationMs));
        timeline.MaxSeekTime(milliseconds(state.canSeek ? state.durationMs : 0));
        timeline.Position(milliseconds(qBound<qint64>(0, state.positionMs, state.durationMs)));
        m_controls.UpdateTimelineProperties(timeline);
    }

    void updateThumbnail(const QUrl &artworkUrl) {
        SystemMediaTransportControls controls = m_controls;
        const std::shared_ptr<std::atomic<quint64>> current = m_thumbnailGeneration;
        const quint64 generation = ++*current;
        if (!artworkUrl.isLocalFile()) {
            controls.DisplayUpdater().Thumbnail(nullptr);
            controls.DisplayUpdater().Update();
            return;
        }
        const auto operation = StorageFile::GetFileFromPathAsync(toHString(QDir::toNativeSeparators(artworkUrl.toLocalFile())));
        operation.Completed([controls, current, generation](const auto &sender, winrt::Windows::Foundation::AsyncStatus status) {
            if (status != winrt::Windows::Foundation::AsyncStatus::Completed || current->load() != generation) {
                return;
            }
            try {
                controls.DisplayUpdater().Thumbnail(RandomAccessStreamReference::CreateFromFile(sender.GetResults()));
                controls.DisplayUpdater().Update();
            } catch (const winrt::hresult_error &) {
            }
        });
    }

    QPointer<MediaSession> m_session;
    SystemMediaTransportControls m_controls{nullptr};
    winrt::event_token m_buttonToken{};
    winrt::event_token m_positionToken{};
    MediaSessionState m_last;
    QElapsedTimer m_timelineClock;
    std::shared_ptr<std::atomic<quint64>> m_thumbnailGeneration = std::make_shared<std::atomic<quint64>>(0);
};

}

std::unique_ptr<MediaSessionBackend> createPlatformMediaSessionBackend(MediaSession *session) { return std::make_unique<WindowsBackend>(session); }
