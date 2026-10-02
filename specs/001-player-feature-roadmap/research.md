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
- Artwork comes from `QMediaMetaData::CoverArtImage`/`ThumbnailImage`. MPRIS needs a URL, so the
  image is written to `<CacheLocation>/nowplaying.png`.

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
- Errors come from `QMediaPlayer::errorOccurred`. A 15-second watchdog covers the time between
  `LoadingMedia` and `LoadedMedia`, to meet SC-010.

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

## Open items

None. Every Technical Context item is resolved above.
