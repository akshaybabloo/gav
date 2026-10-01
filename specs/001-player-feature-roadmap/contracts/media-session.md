# Contract: OS Media Session Integration

GAV publishes its playback state to the OS and accepts commands from it (FR-027, FR-028). The
state is described in the data model: [MediaSessionState](../data-model.md#mediasessionstate-story-4-published-to-the-os).

## Internal interface

`MediaSession` is a `QObject` with one platform implementation chosen at build time. It falls
back to a no-op implementation where the platform has no API or setup fails.

```text
properties written by the app: playbackStatus, title, artist, album, durationMs, positionMs,
                               artworkUrl, canGoNext, canGoPrevious, canSeek
signals emitted to the app:    playRequested, pauseRequested, playPauseRequested, stopRequested,
                               nextRequested, previousRequested, seekRequested(positionMs)
```

- `QML_ELEMENT`/singleton exposure lets `Main.qml` bind properties and connect signals to the same
  handlers the shortcuts use.
- If setup fails, for example because no D-Bus session bus is available, a warning is logged and
  GAV keeps working without a media session.

## Linux: MPRIS2

- Bus name: `org.mpris.MediaPlayer2.gav`. If that name is taken, `org.mpris.MediaPlayer2.gav.instance<pid>`.
- Object path: `/org/mpris/MediaPlayer2`.

**`org.mpris.MediaPlayer2`**

| Member | Value |
|--------|-------|
| `Identity` | `GAV` |
| `DesktopEntry` | `gav` (matches `support/gav.desktop`) |
| `CanQuit` | `true` |
| `CanRaise` | `true` |
| `HasTrackList` | `false` |
| `SupportedUriSchemes` | `file`, `http`, `https` |
| `SupportedMimeTypes` | Derived from `AppConstants` extensions |
| `Raise()` | Shows and activates the main window |
| `Quit()` | Quits the app |

**`org.mpris.MediaPlayer2.Player`**

| Member | Value |
|--------|-------|
| `PlaybackStatus` | `Playing` / `Paused` / `Stopped` |
| `Metadata` | `mpris:trackid` (`/org/gav/track/<n>`), `mpris:length` (µs, omitted when live), `xesam:title`, `xesam:artist` (as a list), `xesam:album`, `mpris:artUrl` |
| `Position` | µs. Not emitted through `PropertiesChanged`; `Seeked` is emitted after jumps of more than 1 s |
| `CanPlay` / `CanPause` / `CanControl` | `true` while media is loaded |
| `CanGoNext` / `CanGoPrevious` | From the playlist |
| `CanSeek` | From `canSeek` |
| `Volume` | Read-only mirror of the player volume. Writes are ignored |
| `Rate` / `MinimumRate` / `MaximumRate` | Read-only mirror of the speed presets |
| Methods | `Play`, `Pause`, `PlayPause`, `Stop`, `Next`, `Previous`, `Seek(offsetUs)`, `SetPosition(trackId, posUs)`, `OpenUri(uri)` (opens a file or http(s) URL) |

`PropertiesChanged` is emitted on `org.freedesktop.DBus.Properties` for every changed property
except `Position`.

## Windows: SystemMediaTransportControls

- Obtained for the main window's HWND via `ISystemMediaTransportControlsInterop::GetForWindow`.
- Enabled buttons: play, pause, stop, next and previous (each set from `canGoNext` and
  `canGoPrevious`).
- `DisplayUpdater`:
  - `Type` is `Video` when media has video, otherwise `Music`.
  - Title, artist and album come from the metadata.
  - The thumbnail is a stream reference to the cached artwork file.
  - `Update()` is called whenever these change.
- `PlaybackStatus` is mapped from `playbackStatus`.
- The timeline (`UpdateTimelineProperties`) is updated about once a second while playing and
  after every seek. It is omitted for live streams.
- `ButtonPressed` and `PlaybackPositionChangeRequested` are marshalled to the GUI thread and
  emitted as signals.

## macOS: MediaPlayer framework

- `MPRemoteCommandCenter` targets:
  - `playCommand`, `pauseCommand`, `togglePlayPauseCommand`, `stopCommand`
  - `nextTrackCommand` and `previousTrackCommand`, enabled from `canGoNext`/`canGoPrevious`
  - `changePlaybackPositionCommand`, enabled from `canSeek`
- `MPNowPlayingInfoCenter.defaultCenter.nowPlayingInfo` sets:
  - `MPMediaItemPropertyTitle`, `MPMediaItemPropertyArtist`, `MPMediaItemPropertyAlbumTitle`
  - `MPMediaItemPropertyPlaybackDuration` (omitted for live streams)
  - `MPNowPlayingInfoPropertyElapsedPlaybackTime`, `MPNowPlayingInfoPropertyPlaybackRate`
  - `MPMediaItemPropertyArtwork` from the cached image
- `playbackState` mirrors `playbackStatus`.
- Command handlers dispatch to the main queue and return `MPRemoteCommandHandlerStatusSuccess`, or
  `…CommandFailed` when no media is loaded.

## Behavioural guarantees

- Commands received with no media loaded are ignored (spec edge case) and return success. Nothing
  changes.
- Commands are acted on within 500 ms of arrival (SC-009).
- GAV never grabs media keys directly. Which app receives them is left to the OS.
