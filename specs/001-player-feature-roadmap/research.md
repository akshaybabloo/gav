# Research: Player Feature Roadmap

**Feature**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md) | **Date**: 2026-10-02

The facts below were checked against the Qt 6.12.0 headers in `~/Qt/6.12.0/gcc_64/include`, the
vcpkg ports at the pinned checkout (`~/vcpkg/ports`), and GAV's current `CMakeLists.txt`,
`vcpkg.json` and `.github/workflows/build.yaml`.

## R1. What Qt Multimedia already gives us

**Findings**

- `QMediaPlayer` exposes `audioTracks()`, `subtitleTracks()`, `activeAudioTrack`,
  `activeSubtitleTrack` and `tracksChanged`/`activeTracksChanged`, with language and title in each
  track's `QMediaMetaData`.
- Embedded subtitles reach the UI only as plain text, through `QVideoSink::subtitleText`, which
  `VideoOutput` draws itself. All ASS styling is lost.
- There is no API for external subtitle files, chapters, embedded fonts (MKV attachments) or frame
  stepping.
- `QMediaPlayer::isSeekable()` plus a zero/unknown `duration()` identify live streams.
- `QVideoFrame::startTime()` gives the exact presentation timestamp of each displayed frame.

**Decision**: Use Qt directly for audio track selection, stream playback and live detection. Use
it for the subtitle *track list* only as a fallback. Everything else (subtitle rendering, external
files, chapters, fonts) needs GAV's own media probing and rendering (R2–R4).

## R2. Full-fidelity ASS rendering (FR-001a, SC-011)

**Decision**: Render all subtitles with **libass 0.17.5** (vcpkg port `libass`). Every subtitle
source (SRT, VTT, ASS/SSA, embedded text tracks) is converted to ASS events and fed into a single
libass track, so there is one rendering path.

- libass output (`ASS_Image` alpha bitmaps + colour) is composited into a premultiplied ARGB
  `QImage` and shown by a `SubtitleOverlay` `QQuickItem` (texture node) above `VideoOutput`.
- Rendering is driven by `QVideoSink::videoFrameChanged`. Each frame renders at
  `frame.startTime() + subtitleDelay`, which keeps subtitles frame-exact with video. libass's
  `detect_change` flag skips work when nothing changed.
- Rendering runs on a worker thread. Only the finished image crosses to the render thread.
- The overlay covers the video's painted rectangle. It is not affected by GAV's zoom/pan, so
  subtitles stay readable while zoomed.
- Font providers: fontconfig on Linux, DirectWrite on Windows, CoreText on macOS (libass
  autodetect). Embedded fonts are added with `ass_add_font`.
- Qt's own subtitle drawing is turned off (`activeSubtitleTrack = -1`) whenever GAV's renderer is
  active.

**Rationale**: libass is the reference ASS renderer used by mpv, VLC and FFmpeg. Nothing else
meets "full fidelity" across platforms.

**Alternatives considered**

- Qt's `subtitleText`: plain text only, which fails FR-001a.
- Burning subtitles in with FFmpeg's `subtitles` filter inside the multimedia plugin: this needs
  invasive patches to qtmultimedia, cannot change delay or size without rebuilding the filter
  graph, and would not exist with Qt's stock plugin during local development.
- Writing our own ASS renderer: far too large, and the result would not match reference players.

**Packaging note (Linux)**: vcpkg's static fontconfig looks for its config under the vcpkg
install prefix, which doesn't exist on users' machines. At startup on Linux, if `FONTCONFIG_FILE`
is unset and `/etc/fonts/fonts.conf` exists, GAV sets `FONTCONFIG_FILE` to it before creating the
libass library. The AppImage relies on the host's `/etc/fonts`. A quickstart scenario checks this.

## R3. Getting subtitle events, chapters and fonts out of media files

**Findings**

- GAV does not link FFmpeg today. It only reads `libavutil/ffversion.h` through
  `GAV_FFMPEG_INCLUDE_DIR`.
- On a plugin-cache hit, CI installs vcpkg deps with `--x-no-default-features`, which skips
  FFmpeg.

**Decision**: Link vcpkg's static `avformat`/`avcodec`/`avutil` into the `gav` executable. Use
them only in a new **probe subprocess** (`GAV_SUBPROCESS=probe`). The main process launches it
with `QProcess` for each opened local file.

- The probe opens the file and first emits metadata: streams, chapters, embedded fonts and
  embedded subtitle track headers. It then streams subtitle events as JSON lines on stdout (see
  [contracts/probe-protocol.md](./contracts/probe-protocol.md)).
- Only subtitle and attachment streams are read (`AVDISCARD_ALL` on the rest). Subtitle packets go
  through FFmpeg's text subtitle decoders, which always produce ASS dialogue lines (`subrip`,
  `webvtt`, `ass`, `mov_text`, …).
- External subtitle files use the same subprocess and protocol, so SRT/VTT/ASS parsing comes from
  FFmpeg rather than hand-written parsers.
- Events are fed to libass progressively (`ass_process_chunk`), so subtitles near the start are
  available within about a second while the rest of a large file is still being read.
- If the probe crashes or exits with an error, GAV logs it, shows a snackbar, and falls back to
  Qt's plain-text rendering of the selected embedded track.

**Rationale**: Constitution Principle II requires untrusted media processing to run in an
isolated subprocess. GAV already has a subprocess mode for collage, so this extends an existing
pattern.

Static libraries in the executable must not be exported. On Linux, `ld` exports any executable
symbol that a shared library on the link line also references, so the static fontconfig and
FreeType pulled in by libass were exported and Qt's own calls into the system fontconfig were
bound to them, crashing the file dialog. `gav_media_deps` therefore links with
`-Wl,--exclude-libs,ALL` on Linux, which keeps every static archive's symbols private: Qt keeps
using the system libraries, libass keeps its own copies, and FFmpeg does not clash with the
multimedia plugin's FFmpeg. macOS (two-level namespace) and Windows (DLL imports) do not have
this interposition problem.

**Alternatives considered**

- In-process probing: violates Principle II, because a malformed file could crash the UI.
- A separate `gav-probe` executable: works, but adds a second binary to every package format for
  no benefit over re-executing `gav`.
- Patching qtmultimedia to expose raw ASS packets: would not work with the stock plugin, and the
  patches would be fragile.

**CI impact**: The `--x-no-default-features` shortcut on plugin-cache hits must go, because FFmpeg
is now needed by GAV itself. vcpkg's binary cache still makes cache hits cheap. The `ffmpeg`
feature description in `vcpkg.json` will be updated to say GAV links it too.

## R4. Subtitle text encoding (edge case: non-UTF-8 files)

**Decision**: The probe subprocess detects encoding with **uchardet** (vcpkg `uchardet`) when the
file has no BOM and isn't valid UTF-8. It passes the result to FFmpeg as `sub_charenc`, which
requires the vcpkg `ffmpeg[iconv]` feature.

**Alternatives considered**

- Qt's `QStringDecoder`: only reliable for UTF and Latin-1 unless Qt is built with ICU, which
  varies by platform.
- Assuming UTF-8 and falling back to Windows-1252: garbles Cyrillic, CJK and Arabic subtitles.

## R5. Sidecar subtitle discovery (FR-003)

**Decision**: Look for files in the video's folder named `<basename>.<ext>` or
`<basename>.<lang>.<ext>`, where `<ext>` is `srt|ass|ssa|vtt` (case-insensitive) and `<lang>` is
any dot-free suffix up to 8 characters (e.g. `en`, `pt-BR`, `forced`).

The default choice is the file matching the preferred subtitle language (FR-010). Otherwise it is
the alphabetically first file. This is a pure function, `SubtitleFiles::discover`, covered by unit
tests.

## R6. Frame stepping (FR-020, SC-007)

**Findings**: Qt 6.12 has no step API.

**Decision**: While paused, `setPosition(position ± frameDuration)`, where `frameDuration` comes
from the stream frame rate already shown in the stats overlay (`CustomMediaPlayer::fps` /
`mediaInfo`). The new position is snapped to the start time of the frame currently displayed
(`QVideoFrame::startTime`) to stop rounding errors building up. Stepping during playback pauses
first.

**Risk**: SC-007 requires 95% accuracy. Qt's FFmpeg backend decodes forward from the previous
keyframe to the exact target when paused, but variable-frame-rate files may drift. Quickstart
scenario Q3.1 measures this. If it falls short, a follow-up task can add a small patch to
qtmultimedia (GAV already maintains patches in `support/`).

**Alternatives considered**: Decoding frames ourselves in the probe process is far too much work
for one feature, and would duplicate the player.

## R7. Persistence: history, recent files, last-session playlist

**Decision**

- **Settings** (subtitle size, preferred languages, remember-positions toggle,
  restore-last-playlist toggle, shuffle state) go into the existing QML `Settings` object in
  `Main.qml`.
- **Playback history** (positions plus the recent-files list) goes into
  `<AppDataLocation>/history.json`, written atomically with `QSaveFile` on a background thread. It
  is capped at 200 position entries and 10 recent entries.
- **Last-session playlist** goes into `<AppDataLocation>/session.m3u8` with a `#GAV-CURRENT:<index>`
  directive, written on exit only when the setting is on.

**Rationale**: History is a list of up to 200 records rewritten whenever a position is saved.
Keeping it out of the settings file avoids bloating it and avoids rewrite contention. Reusing M3U
for the session playlist means one parser and one set of tests. The Constitution allows departing
from Qt Settings when justified, which this plan does in Complexity Tracking.

**Alternatives considered**: QSettings arrays (clumsy, and the whole file is rewritten anyway) or
SQLite (a new dependency for 200 rows).

## R8. Playlist files (FR-018, FR-019)

**Decision**: Write extended M3U as UTF-8 (`#EXTM3U`, `#EXTINF:<seconds>,<title>`, one location per
line) with `.m3u8` as the default extension. Read `.m3u` and `.m3u8`, tolerating:

- a missing `#EXTM3U` header
- CRLF line endings
- a UTF-8 BOM
- `file://` URIs
- `http(s)://` URLs
- relative paths, resolved against the playlist file's folder

Missing local entries are skipped and counted. This is a pure C++ class (`PlaylistIO`) with unit
tests.

## R9. Shuffle (FR-017)

**Decision**: Keep a shuffled order of the playlist indices not yet played, generated with
Fisher–Yates using `QRandomGenerator`. Turning shuffle on mid-playlist keeps the current item and
shuffles only the rest. Adding items inserts them at random positions in the remaining order. When
every item has played, a new order is generated, but only if repeat-playlist is active. This is
pure logic (`ShuffleOrder`) with unit tests.

## R10. OS media keys and now-playing (FR-027, FR-028)

**Decision**: An abstract `MediaSession` with three platform back-ends and a no-op fallback,
chosen at compile time:

| Platform | Mechanism | Build impact |
|----------|-----------|--------------|
| Linux | MPRIS2 over D-Bus (`org.mpris.MediaPlayer2`, `.Player`) via `QtDBus` | Link `Qt6::DBus` on Linux |
| Windows | `SystemMediaTransportControls` via C++/WinRT `ISystemMediaTransportControlsInterop::GetForWindow(hwnd)` | Windows SDK C++/WinRT headers; link `windowsapp.lib` |
| macOS | `MPRemoteCommandCenter` + `MPNowPlayingInfoCenter` (Objective-C++ `.mm`) | Link `MediaPlayer.framework` |

- Hardware media keys reach the app through these OS services on all three platforms, so there's
  no need to grab keys globally.
- Artwork comes from `QMediaMetaData::CoverArtImage`. MPRIS needs a URL, so the
  image is written to `<CacheLocation>/nowplaying-<n>.png` (a new name for each cover, so the URL changes with it).

**Alternatives considered**: Global key hooks such as `RegisterHotKey` or X11 grabs. These conflict
with other players and the OS's normal choice of which app gets the keys, and don't work on
Wayland.

## R11. Network streams and TLS (FR-029–FR-031)

**Findings**: In the vcpkg `ffmpeg` port, Windows builds get `--enable-schannel` automatically
when the `openssl` feature is off. Linux and macOS builds currently have **no TLS**, so HTTPS
streams would fail with the custom plugin.

**Decision**

- Add `ffmpeg[openssl]` for Linux and macOS in `vcpkg.json`. This changes the plugin cache key, so
  the first CI run after it is a cold build.
- `QMediaPlayer::setSource(QUrl("https://…"))` handles HTTP(S) files and HLS.
- Command-line arguments and the single-instance hand-off accept `http`/`https` URLs alongside
  files.
- Errors come from `QMediaPlayer::errorOccurred`. SC-010 is met by setting
  `QPlaybackOptions::networkTimeout` to 10 seconds, which FFmpeg applies to each connection and
  read. A 120-second watchdog between `LoadingMedia` and `LoadedMedia` is only a backstop (R16).

**Alternatives considered**: GnuTLS (larger dependency tree) or Secure Transport on macOS (Apple
has deprecated it).

## R12. Chapter navigation rule (FR-022)

**Decision**: "Previous chapter" goes to the start of the current chapter, or to the previous
chapter if the position is within 3 s of the current chapter's start. "Next chapter" goes to the
next chapter's start and does nothing in the last chapter. This is a pure function in
`PlaybackUtils` with unit tests.

## R13. Go-to-time parsing (FR-023)

**Decision**: Accept `h:mm:ss`, `m:ss`, `mm:ss` and plain seconds, optionally with `.fff`
milliseconds. The minutes and seconds fields must be under 60 when a larger unit is present.
Anything else, or a time beyond the duration, is rejected with a reason code that the UI turns
into an inline message. This is a pure function, `PlaybackUtils::parseTime`.

## R14. Keyboard shortcut conflicts

**Decision**: Use the VLC-style table in FR-024 as written.

- The new single-letter shortcuts are disabled while any `TextField` has active focus (FR-026).
  This is enforced in one place: a `shortcutsEnabled` property in `Main.qml` that every new
  `Shortcut` binds to.
- `Esc` exits full-screen only when no popup or dialog is open. Dialogs handle their own `Esc`
  (e.g. "Start over" in the resume prompt).
- `Ctrl+T`, `Ctrl+O` and `Ctrl+N` keep working while text fields have focus, because they open
  dialogs.

## R15. Buffering indicator and buffered range (FR-031a)

**Findings** (measured against Qt 6.12 with a local HTTP server that stalls mid-file)

- When a stream runs out of data, the FFmpeg backend keeps reporting `BufferedMedia` and
  `PlayingState`; the position simply stops. `BufferingMedia` only appears for the first moments
  after `play()`, while playback is already advancing.
- `bufferProgress` is only ever 0.25 or 1, and `bufferedTimeRange()` is always empty.
- The backend reads ahead at most 4 seconds or 32 MB.

**Decision**: Detect stalls directly. `CustomMediaPlayer::buffering` turns on when the state is
playing and the position has not moved for 750 ms (polled every 250 ms), and off as soon as it
moves, playback stops, or the media ends. The main and mini players show a spinner from it.

**Rejected**: A buffered-range bar on the seek slider. Qt exposes no range, and a 4-second
read-ahead would be an invisible sliver on most videos. It would need a qtmultimedia patch that
exposes the demuxer's buffered duration and raises its limit, which would not work with Qt's
stock plugin in local development.

## R16. HLS master playlists and the load deadline

**Findings** (measured against Qt 6.12 with a public eight-variant HLS master playlist on a slow
server)

- FFmpeg opens and probes every variant before the media counts as loaded. That took about 40
  seconds, so the original 15-second overall watchdog stopped a stream that was loading normally.
- A dead address or a server that accepts and never answers fails in about 10 seconds through the
  network timeout alone.
- After loading, Qt plays the first video and audio track (the lowest quality in that playlist)
  and never marks the other streams as discarded, so FFmpeg keeps downloading every variant.
  Playback advanced about 8 seconds in 45. A single variant's playlist loads in about 5 seconds
  and plays close to real time.

**Decision**: The per-operation network timeout carries SC-010 and the overall watchdog becomes a
120-second backstop. An "Opening stream…" indicator shows while a network source is loading.

**Follow-up**: R18 makes GAV choose one variant itself.

## R17. Remote playlists

**Findings**: FFmpeg only understands HLS at an `.m3u`/`.m3u8` address, so a plain list of channels
or files (for example an IPTV index with about 11 000 entries, 2.5 MB) fails with "Could not open
file". Such lists carry attributes on `#EXTINF` lines whose quoted values contain commas.

**Decision**: When the user opens an `http(s)` address ending in `.m3u` or `.m3u8`, GAV downloads
it with Qt Network (already linked; 10-second timeout, 32 MB cap). A document containing
`#EXT-X-` tags is HLS and is played as one stream. A document with an `#EXTM3U` header or
`#EXTINF` lines is parsed as a playlist, and anything else is rejected as not a playlist: relative
entries resolve against the address after redirects, and entries that are not `http(s)` are
skipped so a remote list cannot point at local files. The `#EXTINF` title is whatever follows the
first comma outside quotes, and it becomes the item name. If the download fails the TLS handshake,
or no TLS backend is available, the address is handed to the player unchanged.

**Not covered**: per-entry options such as `#EXTVLCOPT` and `http-user-agent`, so channels that
need them will not play.

## R18. Stream quality selection (FR-031b)

**Findings**

- Qt offers no way to make FFmpeg skip the unselected variants of a master playlist, and FFmpeg
  refuses a rewritten one-variant master handed over as a `data:` address or a local file.
- Handing FFmpeg a single variant's own playlist works: the eight-variant test stream starts in
  about 9 seconds instead of 45 and plays in real time.
- Nothing in Qt reports connection speed. `QNetworkInformation` only gives the transport type.
- `QMediaPlayer::setSource` stops the old media first, which reports `LoadedMedia` for it, so
  resume state has to be armed after the call.

**Decision**: For an `http(s)` address ending in `.m3u8` or `.m3u`, `StreamQuality` downloads the
master playlist and `Hls::parseMaster` lists every video rendition (best first, labelled by height,
with the codec or bitrate added when a height occurs more than once) and every audio group as an
audio format (AAC first, named from the codec and channel count). Nothing is merged or dropped
from the lists. The player's `source` stays the address the user opened.

- When audio is muxed into the variants, Qt plays the chosen variant's own playlist.
- When audio comes from separate playlists, `Hls::playback` builds a master playlist that holds
  only the chosen video rendition and the chosen audio group (all of its languages, default
  first, addresses made absolute). Qt reads it from memory through
  `QMediaPlayer::setSourceDevice`, with the opened address as the name so FFmpeg recognises HLS.
  A 65-variant sample (13 video renditions, 5 audio formats) starts in about 2 seconds this way.

Automatic choice downloads the start of one segment of the middle variant for up to 1.5 seconds
and picks the best variant whose `BANDWIDTH` × 1.5 fits the measured rate and whose height fits
the screen. If the measurement fails, the middle variant is used. A manual choice lasts for the
current stream. Switching quality or audio format reloads and restores the position and play
state.

**Limits**

- Streams whose address has no `.m3u8`/`.m3u` ending are not inspected.
- The quality does not adapt during playback.
- Seek-bar previews are off while a stream plays from an in-memory playlist.

## R19. Subtitles on HLS streams (FR-031c)

**Findings**

- A master playlist lists WebVTT subtitle tracks as `#EXT-X-MEDIA:TYPE=SUBTITLES` entries, each
  with its own playlist address. The probe's FFmpeg opens such an address directly and returns
  every cue with the time written in the file. It downloads all segments while opening, so the
  cues arrive together (about 4 seconds for 100 segments from one server, 19 from another).
- For on-demand streams those cue times match the player position. For a live channel they are
  relative to the current clip while the player position starts at zero when the stream is opened,
  and Qt does not expose the stream's start timestamp, so they cannot be lined up.

**Decision**: `Hls::parseMaster` collects the subtitle tracks (http(s) addresses only).
`CustomMediaPlayer` hands them to `SubtitleController::setStreamTracks` once the media has loaded
with a duration and is seekable. A track is fetched when the user selects it:
`MediaProbe::loadSubtitleFile` accepts an address, the probe skips encoding detection and stream
analysis for it, and the inactivity watchdog is 120 seconds for these loads. Rendering, delay and
size work as for any other subtitle track. Tracks are not selected automatically.

**Also changed**: an audio group counts as separate audio only when its default entry has its own
playlist, so streams whose default audio is muxed (with an alternate track on the side) get
quality selection.

**Limits**: no subtitles on live streams, none for closed captions carried inside the video, and
cue positioning from the WebVTT file is not applied.

## Open items

None. Every Technical Context item is resolved above.
