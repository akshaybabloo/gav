---

description: "Task list for the Player Feature Roadmap"
---

# Tasks: Player Feature Roadmap

**Input**: Design documents from `specs/001-player-feature-roadmap/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md)

**Tests**: Included. Constitution Principle V requires unit tests for all non-GUI logic, and
[quickstart.md](./quickstart.md#automated-checks) lists the required test files. UI behaviour is
checked by the manual quickstart scenarios referenced at each checkpoint.

**Organization**: Tasks are grouped by user story, so each story is one independently shippable
PR on top of the Foundational phase ([plan.md → Delivery Order](./plan.md#delivery-order)).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on unfinished tasks)
- **[Story]**: The user story the task belongs to (US1–US4)

## Path Conventions

GAV keeps every source file at the repository root. QML files belong to the `gavqml` module, and
C++ units are registered in `qt_add_qml_module(appgav … SOURCES …)` in `CMakeLists.txt`. Tests
live in `tests/` and are compiled into the `gav_tests` target. Code follows the surrounding style
with no comments by default (Constitution: Development Workflow).

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Dependencies, build wiring and test fixtures that every story needs

- [X] T001 Update `vcpkg.json`:
  - Add top-level dependencies `libass` and `uchardet`.
  - In the `ffmpeg` feature, add the ffmpeg features `iconv` (all platforms) and `openssl` with `"platform": "linux | osx"`.
  - Update the `ffmpeg` feature description to say FFmpeg is also linked into `gav` for the probe subprocess (research R3, R4, R11).
- [X] T002 Update `CMakeLists.txt` so `appgav` links FFmpeg (`avformat`, `avcodec`, `avutil`) through vcpkg's `find_package(FFMPEG REQUIRED)` wrapper (`FFMPEG_INCLUDE_DIRS`/`FFMPEG_LIBRARIES`).
  - Also link libass (via `PkgConfig` `pkg_check_modules(LIBASS REQUIRED IMPORTED_TARGET libass)` unless the port's `usage` file says otherwise) and uchardet (the CMake config target named in `~/vcpkg/ports/uchardet/usage`).
  - Keep the existing `GAV_FFMPEG_INCLUDE_DIR` lookup working.
  - Do not set `ENABLE_EXPORTS` on `appgav` (research R3: FFmpeg symbols must stay unexported).
- [X] T003 Update `.github/workflows/build.yaml`:
  - Remove the plugin-cache-hit shortcut that appends `--x-no-default-features` (line ~172), so FFmpeg is always installed for `gav` itself.
  - Keep the vcpkg binary cache keys as they are.
  - Confirm `GAV_FFMPEG_INCLUDE_DIR` is still passed or found.
- [X] T004 [P] Create test fixtures in `tests/data/`, with a `tests/data/README.md` giving the exact `ffmpeg`/`mkvmerge` commands used to generate them:
  - `multi.mkv`: 30 s, 25 fps constant frame rate, 2 audio tracks tagged `eng`/`jpn`, 1 ASS subtitle track titled "Signs" tagged `eng` with an attached TTF font, 3 chapters titled "One", "Two", "Three"
  - `multi.en.srt`: UTF-8
  - `multi.fr.srt`: Windows-1252 with accented characters
  - `styled.ass` + `styled.mp4`: one positioned sign, one rotated sign, one karaoke line
  - `playlist-relative.m3u8`: 2 relative entries pointing at the fixtures plus 1 missing entry
  - `corrupt.mkv`: truncated header
  - `long.mp4` (30 min) is not committed. The README explains how to generate it locally for quickstart Q2.x.
- [X] T005 Add a `GAV_TEST_DATA_DIR` compile definition (`${CMAKE_SOURCE_DIR}/tests/data`) and link FFmpeg, libass and uchardet to `gav_tests` in `CMakeLists.txt`, so later test tasks can just append their sources.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Pure-logic units and shared QML helpers used by several stories ([plan.md → Delivery Order](./plan.md#delivery-order), step 0)

**⚠️ CRITICAL**: No user story work starts until this phase is done

- [X] T006 [P] Create `playbackutils.h`/`playbackutils.cpp` with a `PlaybackUtils` QObject (`QML_ELEMENT`, `QML_SINGLETON`) exposing these `Q_INVOKABLE` functions:
  - `parseTime(QString text, qint64 durationMs) → QVariantMap {ok, ms, error}`:
    - Accepts `h:mm:ss`, `m:ss`, `mm:ss` and plain seconds, each with optional `.fff`.
    - Minutes and seconds fields must be under 60 when a larger unit is present.
    - Error codes are `malformed` and `outOfRange` (research R13).
  - `chapterTarget(QVariantList chapters, qint64 positionMs, int direction) → qint64` (−1 when there's no target):
    - Previous goes to the current chapter's start, or to the previous chapter "if the position is within 3 s of the current chapter's start".
    - Next goes to the next chapter's start and does nothing in the last chapter (research R12).
  - `isResumeEligible(qint64 positionMs, qint64 durationMs) → bool`: true only when `0.05 × duration < position < 0.95 × duration` (FR-011).
- [X] T007 [P] Write `tests/test_playbackutils.cpp`, covering:
  - every accepted time form (`1:05`, `65`, `1:05.250`, `1:02:03`)
  - rejections (`1:75`, `9:99:99`, `abc`, negative, past the duration)
  - chapter previous/next at the boundaries (0 s, exactly 3 s, the last chapter, no chapters)
  - the resume window edges at exactly 5% and 95%
- [X] T008 [P] Create `playbackhistory.h`/`playbackhistory.cpp` with a `PlaybackHistory` QObject (`QML_ELEMENT`, `QML_SINGLETON`) backed by `<AppDataLocation>/history.json`, using the shape in [data-model.md](./data-model.md#recentfile-story-2-persisted-in-historyjson) (`"version": 1`, `positions[]`, `recent[]`).
  - `Q_INVOKABLE` API: `savedPosition(path) → qint64` (−1 if none), `recordPosition(path, positionMs, durationMs)`, `removePosition(path)`, `recordOpened(pathOrUrl)`, `recentFiles() → QStringList`, `removeRecent(path)`, `clear()`, and a `recentChanged` signal.
  - "Keep at most 200 entries, evicting the oldest `lastPlayed` first" (FR-013).
  - Recent files: "Up to 10 entries, newest first, without duplicates".
  - "Network URLs never get entries" for positions.
  - Writes use `QSaveFile` via `QtConcurrent::run` so they never block the UI thread.
  - On a parse failure or unknown `version`, log with spdlog, rename to `history.json.bak`, and start empty.
  - The constructor takes an optional storage path so tests can use a temporary directory.
- [X] T009 [P] Write `tests/test_playbackhistory.cpp`, covering:
  - eviction at 201 entries
  - recent-list de-duplication and the cap of 10
  - a URL never storing a position
  - a round trip through a temp directory
  - recovery from a corrupt file, including the `.bak` file being created
  - `clear()` emptying both lists
- [X] T010 [P] Create `playlistio.h`/`playlistio.cpp`, following [contracts/playlist-format.md](./contracts/playlist-format.md):
  - A plain `PlaylistDocument`/`PlaylistEntry` struct per [data-model.md](./data-model.md#playlistdocument-story-2).
  - `PlaylistIO::read(path, supportedExtensions) → {entries, currentIndex, skippedMissing, skippedUnsupported}`.
  - `PlaylistIO::write(path, document)`, which writes UTF-8 without a BOM and `#GAV-CURRENT:<n>` only when `currentIndex` is set.
  - A QObject wrapper `PlaylistFiles` (`QML_ELEMENT`, `QML_SINGLETON`) with asynchronous `Q_INVOKABLE load(QUrl, QStringList supportedExtensions, QString tag)` and `save(QUrl, QVariantList items, int currentIndex, QString tag)`. Both run on `QtConcurrent` and report back with `loaded(tag, QVariantMap result)` and `saved(tag, bool ok)` signals, so playlist I/O and existence checks never run on the UI thread (Constitution Principle II). Items are objects `{name, path}`, matching the existing `playList` model in `Main.qml`.
- [X] T011 [P] Write `tests/test_playlistio.cpp`, covering:
  - a round trip of 100 entries (SC-006)
  - relative path resolution
  - BOM and CRLF input
  - a missing `#EXTM3U` header
  - `file://` and `https://` entries
  - unsupported schemes and extensions counted
  - missing files counted
  - `#GAV-CURRENT` clamped when entries are skipped
- [X] T012 Register `playbackutils`, `playbackhistory` and `playlistio` sources in `qt_add_qml_module` and in `gav_tests`, and add `tests/test_playbackutils.cpp`, `tests/test_playbackhistory.cpp` and `tests/test_playlistio.cpp` to `gav_tests` in `CMakeLists.txt`.
- [X] T013 [P] Add to `AppConstants.qml`:
  - `subtitleExtensions: ["srt","ass","ssa","vtt"]`
  - `playlistExtensions: ["m3u","m3u8"]`
  - `subtitleDelayStep: 100`
  - `subtitleDelayLimit: 600000`
  - `subtitleScaleMin: 0.5`, `subtitleScaleMax: 3.0`, `subtitleScaleStep: 0.1`
  - `streamLoadTimeout: 15000`
  - `chapterPreviousThreshold: 3000`
- [X] T014 Add a reusable on-screen indicator to `MediaComponent.qml`: a `function showOsd(text)` that reuses the existing zoom indicator's style and `AppConstants.volumeDisplayDuration` auto-hide. Stories use it for the subtitle delay, track name, speed, volume and mute messages ([contracts/keyboard-and-cli.md](./contracts/keyboard-and-cli.md)).
- [X] T015 Add `readonly property bool shortcutsEnabled` to `Main.qml`. It is `false` when `activeFocusItem` is a text input (`TextInput`/`TextField`/`TextArea`) or when any `Popup`/`Dialog` is open (research R14, FR-026). Bind `enabled` on the existing Space/Left/Right/I `Shortcut`s to it.

**Checkpoint**: `ctest --output-on-failure` passes, the app builds and runs unchanged on all CI targets, and user stories can start

---

## Phase 3: User Story 1 - Subtitles and track selection (Priority: P1) 🎯 MVP

**Goal**: Full-fidelity subtitles from sidecar, loaded or embedded sources, plus audio and subtitle track switching, subtitle delay and subtitle size (FR-001 to FR-010).

**Independent Test**: Quickstart scenarios Q1.1–Q1.13 using `tests/data/multi.mkv` and `styled.mp4`.

### Tests for User Story 1

- [X] T016 [P] [US1] Write `tests/test_subtitlefiles.cpp`, covering:
  - `movie.srt`, `movie.en.srt`, `movie.pt-BR.vtt` and `MOVIE.EN.SRT` (case-insensitive) are matched
  - `movie.toolongsuffix.srt` (over 8 characters) and `other.srt` are rejected
  - preferred-language choice, with an alphabetical fallback when nothing matches
- [X] T017 [P] [US1] Write `tests/test_probeprotocol.cpp` for `ProbeMessage::parse`/`serialise`:
  - every message type in [contracts/probe-protocol.md](./contracts/probe-protocol.md) round-trips
  - unknown `type` and unknown fields are ignored
  - events with negative times are rejected
  - malformed JSON returns an invalid message
- [X] T018 [P] [US1] Add fixture tests to `tests/test_probeprotocol.cpp`:
  - run `ProbeWorker` in-process against `multi.mkv` and against `multi.fr.srt` (with `--events all`)
  - assert the chapters, streams, fonts, headers, event timings and decoded text (field assertions instead of stored `.jsonl` files, so FFmpeg header comments and temp paths don't break them)
  - check that `corrupt.mkv` produces `error` code `open-failed`, and that sidecars still load when the media can't be opened
- [X] T019 [P] [US1] Write `tests/test_subtitlerenderer.cpp`:
  - load `styled.ass` into `SubtitleRenderer` at 1920×1080 and render at 5 000, 12 000 and 20 000 ms
  - assert non-transparent pixels exist inside the expected bounding boxes for the positioned sign, the rotated sign and the karaoke line, and that a box elsewhere is transparent
  - measure and log the average render time per frame (plan target: under 4 ms)

### Implementation for User Story 1

- [X] T020 [P] [US1] Create `subtitlefiles.h`/`subtitlefiles.cpp` with `SubtitleFiles::discover(videoPath) → QList<{path, language}>` and `SubtitleFiles::choose(list, preferredLanguage) → int`:
  - matches `<basename>.<ext>` and `<basename>.<lang>.<ext>`, where `<ext>` is `srt|ass|ssa|vtt` (case-insensitive) and `<lang>` is "any dot-free suffix up to 8 characters" (research R5)
- [X] T021 [P] [US1] Create `probemessage.h`/`probemessage.cpp`: a `ProbeMessage` variant type (`Hello`, `Media`, `Header`, `Event`, `End`, `Warning`, `Error`) with `parse(QByteArray line)` and `serialise()`, implementing [contracts/probe-protocol.md](./contracts/probe-protocol.md) with `protocol: 1`.
- [X] T022 [US1] Create `probeworker.h`/`probeworker.cpp` (child side, depends on T021):
  - Open the file with `avformat_open_input`/`avformat_find_stream_info`.
  - Send `hello`, then `media`:
    - `durationMs`
    - chapters sorted with overlaps removed, titles defaulting to `Chapter N+1`
    - text-only subtitle streams (drop bitmap codecs PGS/VobSub/DVB)
    - font attachments written to `--fonts-dir` and sent as `{name, path}`, with a `font-write-failed` `warning` for each font that couldn't be written
  - Send `progress` with the current byte offset at least every 2 s while reading.
  - Set `AVDISCARD_ALL` on non-subtitle streams.
  - For each subtitle stream, open its decoder, send `header` with `AVCodecContext::subtitle_header`, decode packets with `avcodec_decode_subtitle2`, and send `event` lines from `AVSubtitleRect::ass`, then `end`.
  - Handle `--subtitle-file` inputs the same way, using `external:<absPath>` sources.
  - Detect encoding with uchardet when there's no BOM and the content isn't valid UTF-8, and pass it as `sub_charenc`, reporting `encoding` in `header` (research R4).
  - Map failures to the error codes `open-failed`, `no-streams`, `decode-failed`, `encoding-failed` and `internal`, with exit code 1.
  - Write only protocol lines to stdout and send logs to stderr.
- [X] T023 [US1] In `main.cpp`, before any GUI object is created:
  - when `GAV_SUBPROCESS=probe`, build a `QCoreApplication`, parse the hidden `--probe`, `--subtitle-file` (repeatable) and `--events` options, run `ProbeWorker`, and return its exit code
  - hide these options from `--help` ([contracts/keyboard-and-cli.md](./contracts/keyboard-and-cli.md))
  - make sure the existing collage check `qEnvironmentVariableIsSet("GAV_SUBPROCESS")` does not match `probe` mode
- [X] T024 [US1] Create `mediaprobe.h`/`mediaprobe.cpp` (parent side, depends on T021):
  - Starts `QCoreApplication::applicationFilePath()` with `GAV_SUBPROCESS=probe` through `QProcess`.
  - Parses lines on `readyReadStandardOutput` and forwards stderr to spdlog at debug level.
  - Creates a private temporary fonts directory per probe, passes it as `--fonts-dir`, and deletes it on `stop()`.
  - Watchdogs: kill the process if `hello` hasn't arrived within 5 s, or if no message (including `progress`) has arrived for 30 s.
  - Signals: `mediaReady(chapters, subtitleStreams, fonts)`, `headerReady(source, assHeader)`, `eventsReady(source, QList<event>)`, `sourceEnded(source)`, `failed(code, detail)`.
  - `start(path, extraSubtitleFiles)`, `loadSubtitleFile(path)` (short-lived probe with `--events all`), and `stop()`, which kills running probes.
  - Treat all fields as untrusted and range-check them.
- [X] T025 [P] [US1] Create `subtitlerenderer.h`/`subtitlerenderer.cpp`, a thread-safe libass wrapper:
  - `ass_library_init` with `ass_set_fonts(…, ASS_FONTPROVIDER_AUTODETECT …)` and `addFont(name, data)` → `ass_add_font`.
  - `setTrack(header)` → `ass_new_track` + `ass_process_codec_private`, and `addEvents(list)` → `ass_process_chunk`.
  - `setFrameSize(w, h)` and `setScale(real)` → `ass_set_font_scale`, using "0.5–3.0 in 0.1 steps".
  - `render(qint64 timeMs) → QImage` (premultiplied ARGB, compositing the `ASS_Image` list), using `detect_change` to return the cached image when nothing changed.
  - `clear()`.
- [X] T026 [US1] Create `subtitleoverlay.h`/`subtitleoverlay.cpp`, a `SubtitleOverlay` `QQuickItem` (`QML_ELEMENT`):
  - Properties: `videoSink` (`QVideoSink*`), `renderer` (`SubtitleRenderer*`), `delayMs`, `contentRect` (the video's painted rectangle).
  - On `QVideoSink::videoFrameChanged`, render at `frame.startTime()/1000 + delayMs` on a worker thread (dropping requests while one is in flight), then call `update()`.
  - `updatePaintNode` uses a `QSGSimpleTextureNode` positioned at `contentRect`.
  - The overlay is not affected by zoom or pan (research R2).
- [X] T027 [US1] In `main.cpp`, on Linux only: before the `QGuiApplication`/libass is created, if `FONTCONFIG_FILE` is unset and `/etc/fonts/fonts.conf` exists, set `FONTCONFIG_FILE` to it (research R2).
- [X] T028 [US1] Extend `custommediaplayer.h`/`custommediaplayer.cpp` (depends on T020, T024, T025). The subtitle state lives in a new `SubtitleController` (`subtitlecontroller.h`/`.cpp`) owned by the player and exposed as `subtitles`; audio tracks and `chapters` stay on the player:
  - Own a `MediaProbe` and a `SubtitleRenderer`, and expose the renderer as a `Q_PROPERTY`.
  - On a local-file `source` change: stop the old probe, reset `subtitleDelay` to 0, discover sidecars on a `QtConcurrent` worker, then start the probe with them. Load embedded fonts from the probe's font paths on a worker thread before calling `SubtitleRenderer::addFont`.
  - Expose `subtitleTracks` (QVariantList of `{id, origin, language, title, displayName, codec, loadState, isDefault, isForced}` per [data-model.md](./data-model.md#subtitletrack-story-1)), with `displayName` following "`title (language)`, then `language`, then `Track N`" (FR-007).
  - Expose `activeSubtitleTrackId` (empty means Off), `audioTracks` (from `QMediaPlayer::audioTracks()`), `activeAudioTrack`, `subtitleDelay` ("±100 ms" steps, "clamped to ±600 000 ms") and `chapters`, each with change signals.
  - Turn off Qt's own subtitle drawing (`setActiveSubtitleTrack(-1)`) while GAV renders.
- [X] T029 [US1] Add track selection logic to `custommediaplayer.cpp`:
  - A track switch requested before `LoadedMedia` is queued and applied once loading completes (spec edge case).
  - Auto-select on load: preferred subtitle language, then forced or default disposition, then the chosen sidecar, then Off. Audio follows the preferred audio language (FR-010).
  - `Q_INVOKABLE selectSubtitleTrack(id)`, `cycleSubtitleTrack()` (includes Off), `selectAudioTrack(index)` (preserving position) and `cycleAudioTrack()`.
  - `Q_INVOKABLE loadSubtitleFile(QUrl)` (FR-004), which rejects extensions not in `AppConstants.subtitleExtensions`.
- [X] T030 [US1] Add probe failure handling to `custommediaplayer.cpp`:
  - On `MediaProbe::failed` or a crash, mark `Streaming` tracks `Failed` and emit `subtitleError(message)`.
  - For an embedded active track, switch to `FallbackPlainText` by calling Qt's `setActiveSubtitleTrack` with the matching Qt index, so Qt draws plain text.
  - External failed tracks stay listed but disabled ([data-model.md](./data-model.md#subtitletrack-story-1) state machine).
- [X] T031 [P] [US1] Create `TrackMenu.qml`, a `Menu`/`Popup` containing:
  - a subtitle section with each track's `displayName` plus Off
  - "Load subtitle file…", which opens a `FileDialog` filtered to `AppConstants.subtitleExtensions`
  - an audio section
  - subtitle delay −/+ buttons with the current value
  - a subtitle size slider (`subtitleScaleMin`–`subtitleScaleMax`, step `subtitleScaleStep`)
  - When there are no subtitle tracks, the section shows a disabled "No subtitles" row. With one audio track, it shows a disabled "Single audio track" row (acceptance scenario 1.7).
- [X] T032 [US1] Add a track-menu button to `MediaControlsComponent.qml` that opens `TrackMenu`, visible when `player.hasVideo` or there is more than one audio track, matching the existing button style and accessibility attributes.
- [X] T033 [US1] Add a `SubtitleOverlay` above `VideoOutput` in `MediaComponent.qml`:
  - `videoSink: videoOutput.videoSink`, `contentRect: videoOutput.contentRect`, `renderer: mediaPlayer.subtitleRenderer`, `delayMs: mediaPlayer.subtitleDelay`
  - extend the drop handler so dropped `.srt/.ass/.ssa/.vtt` files call `loadSubtitleFile` instead of joining the playlist
- [X] T034 [US1] Add a `SubtitleOverlay` over `miniVideoOutput` in `MiniPlayerWindow.qml`, bound to the same player and renderer (FR-033).
- [X] T035 [US1] In `Main.qml`:
  - add `subtitleScale` (default 1.0), `preferredSubtitleLanguage` and `preferredAudioLanguage` (default empty) to `appSettings`, and bind them to the player
  - show `subtitleError` messages in the snackbar
  - add `Shortcut`s (bound to `shortcutsEnabled`) for `V` (cycle subtitle), `B` (cycle audio), `G` (delay −100 ms) and `H` (delay +100 ms), each calling `mediaComponent.showOsd(…)` with e.g. `Subtitle delay: +300 ms` or the track name
- [X] T036 [US1] Add preferred subtitle and audio language fields (ISO 639 codes, can be empty) to `SettingsDialog.qml`, and add the V/B/G/H rows to the "Keyboard Shortcuts" grid.
- [ ] T077 [US1] Package check for US1: build DEB, AppImage, NSIS and DMG from the US1 branch and confirm each starts and renders `multi.en.srt` (quickstart Q1.1, Q1.11), fixing any deploy gaps for libass and its font libraries in `support/cpack.cmake` (Constitution Principle III).
- [X] T037 [US1] Register `subtitlefiles`, `probemessage`, `probeworker`, `mediaprobe`, `subtitlerenderer` and `subtitleoverlay` in `qt_add_qml_module`, add `TrackMenu.qml` to `QML_FILES`, and add the pure units plus `tests/test_subtitlefiles.cpp`, `tests/test_probeprotocol.cpp` and `tests/test_subtitlerenderer.cpp` to `gav_tests` in `CMakeLists.txt`.

**Checkpoint**: ctest passes, and quickstart Q1.1–Q1.13 pass on Linux, Windows and macOS. This is the MVP and ships as its own PR.

---

## Phase 4: User Story 2 - Playback continuity (Priority: P2)

**Goal**: A resume prompt, a recent files list, shuffle, playlist save and open, optional session restore, and clearing history (FR-011 to FR-019a).

**Independent Test**: Quickstart scenarios Q2.1–Q2.12.

### Tests for User Story 2

- [X] T038 [P] [US2] Write `tests/test_shuffleorder.cpp`, covering:
  - every index played exactly once per cycle for sizes 1, 2 and 50
  - turning shuffle on mid-playlist keeps the current item
  - removal and move remapping
  - items inserted mid-cycle get played
  - a new cycle starts only when `repeatPlaylist` is true

### Implementation for User Story 2

- [X] T039 [P] [US2] Create `shuffleorder.h`/`shuffleorder.cpp`, a `ShuffleOrder` QObject (`QML_ELEMENT`) using Fisher–Yates with `QRandomGenerator` (research R9):
  - `Q_INVOKABLE reset(int count, int currentIndex)`, `next(bool repeatPlaylist) → int` (−1 when done), `previous() → int`, `itemInserted(int index)`, `itemRemoved(int index)` and `itemMoved(int from, int to)`
  - `enabled` property
- [X] T040 [P] [US2] Create `ResumeDialog.qml`, a modal `Dialog` titled "Resume from <time>?" (formatted with `AppConstants.formatTime`), with Resume (default, Enter) and Start over (Esc) buttons, and signals `resumeChosen()`/`startOverChosen()` (FR-012, [data-model.md ResumePrompt](./data-model.md#resumeprompt-story-2-ui-state)).
- [X] T041 [US2] Wire resume into `Main.qml` (depends on T040). When a local file reaches `LoadedMedia`:
  - If `appSettings.rememberPositions` is on, `PlaybackHistory.savedPosition(path)` is at least 0, and `savedPosition < duration`: hold playback paused at the saved position and open `ResumeDialog`. Resume means play. Start over means seek to 0, `removePosition` and play.
  - If the saved position is at or past the duration, discard it silently.
  - If another file opens while the dialog is up, close it without changes.
  - Playlist auto-advance also waits for the dialog (spec edge case).
- [X] T042 [US2] Record positions in `Main.qml`. On stop, before the source changes, and in `onClosing`:
  - call `PlaybackHistory.recordPosition` when `PlaybackUtils.isResumeEligible(position, duration)`
  - otherwise call `removePosition` when the position is in the last 5%
  - skip `http(s)` sources and skip entirely when `rememberPositions` is off (FR-011, FR-014)
  - call `PlaybackHistory.recordOpened` on every successful open
- [X] T043 [P] [US2] Create `RecentFilesMenu.qml`, a `Menu` built from `PlaybackHistory.recentFiles()` (refreshed on `recentChanged`):
  - Choosing an existing file calls `mainWindow.openUrls([url])`.
  - Choosing a missing file shows the snackbar message "File not found" with a "Remove from list" action that calls `removeRecent`.
  - Ends with a "Clear recent files" item.
- [X] T044 [US2] Add "Open Recent" (a `RecentFilesMenu` submenu, two clicks from the main window per SC-005), "Open Playlist…" and "Save Playlist…" to `fileMenu` in `TitleBar.qml`, as new signals handled in `Main.qml`.
- [X] T045 [US2] Add playlist save and open to `Main.qml`:
  - `FileDialog`s filtered to `AppConstants.playlistExtensions` (save defaults to `.m3u8`)
  - save calls `PlaylistFiles.save`
  - open calls `PlaylistFiles.load`, replaces `playList` with entries built through `getMediaInfo`, and shows "Loaded N items, skipped M (missing or unsupported)" when either skipped count is above zero (FR-018, FR-019)
  - `openUrls` treats dropped or passed `.m3u/.m3u8` files as playlists to load rather than media
- [X] T046 [US2] Add a shuffle toggle button to `PlayListComponent.qml`, bound to `appSettings.shuffle`, and keep a `ShuffleOrder` in sync with `playList` changes (insert, remove and drag-reorder hooks).
- [X] T047 [US2] Route next/previous through shuffle in `Main.qml` and `MediaControlsComponent.qml` (depends on T039, T046):
  - make the `nextTrack`/`previousTrack` handlers in `Main.qml` and the `EndOfMedia` branch in `MediaControlsComponent.qml` use `ShuffleOrder.next()`/`previous()` when shuffle is on
  - when it's off, keep the current sequential behaviour (FR-017)
- [X] T048 [US2] Add session restore to `Main.qml`:
  - Add `restoreLastPlaylist: false` to `appSettings`.
  - In `onClosing`, when it's on and `playList.count > 0`, write `<AppDataLocation>/session.m3u8` with `#GAV-CURRENT` via `PlaylistFiles.save`. When it's off, delete that file.
  - In `Component.onCompleted`, restore it before `openUrls(InstanceManager.takePendingUrls())`, selecting the current item without playing. Pending files are then appended and the first one is played (FR-019a, spec edge case).
  - If the restored current item is an `http(s)` URL, highlight it in the playlist but do not set it as the player source until the user presses play, so GAV makes no network connection at startup (Constitution Principle I).
- [X] T049 [US2] Add to `SettingsDialog.qml`:
  - a "Remember playback position" switch (`rememberPositions`, default on)
  - a "Restore last playlist on startup" switch (`restoreLastPlaylist`, default off)
  - a "Clear history" button that calls `PlaybackHistory.clear()` and deletes `session.m3u8`, then confirms with the snackbar (FR-014, FR-016)
- [X] T050 [US2] Register `shuffleorder` in `qt_add_qml_module` and `gav_tests`, add `ResumeDialog.qml` and `RecentFilesMenu.qml` to `QML_FILES`, and add `tests/test_shuffleorder.cpp` in `CMakeLists.txt`.

**Checkpoint**: ctest passes and quickstart Q2.1–Q2.12 pass. This ships as its own PR.

---

## Phase 5: User Story 3 - Precise navigation (Priority: P3)

**Goal**: Frame stepping, chapter markers and navigation, go to time, and the full VLC-style shortcut set with a reference generated from one list (FR-020 to FR-026).

**Independent Test**: Quickstart scenarios Q3.1–Q3.7.

**Dependency note**: Chapter tasks (T053, T054) read `CustomMediaPlayer.chapters`, which US1 fills from the probe (T028). Frame stepping, go to time and the shortcuts do not depend on US1.

### Implementation for User Story 3

- [ ] T051 [US3] Add `Q_INVOKABLE stepFrame(int direction)` to `custommediaplayer.h`/`custommediaplayer.cpp`:
  - Pause first if playing.
  - Frame duration is `1000 / fps`, using the stream frame rate from `mediaInfo`.
  - Target is the last displayed `QVideoFrame::startTime()` ± one frame duration. Clamp to `[0, duration]` and do nothing at frame 0 going backwards (research R6, FR-020).
  - Log the requested and resulting frame times at debug level, for measuring SC-007.
- [ ] T052 [US3] Add `Q_INVOKABLE nextChapter()`/`previousChapter()` to `custommediaplayer.cpp`, using `PlaybackUtils::chapterTarget` with the `chapters` property (FR-022). Both do nothing when there are no chapters.
- [ ] T053 [P] [US3] Draw chapter markers in `SeekBarComponent.qml`:
  - draw a thin tick at `startMs / duration` for each entry in `player.chapters` except the first
  - hovering a tick shows a `ToolTip` with the chapter title, which works alongside the existing hover preview (FR-021)
- [ ] T054 [US3] Add previous/next chapter buttons to `MediaControlsComponent.qml`, visible only when `player.chapters.length > 0`, calling `previousChapter()`/`nextChapter()`.
- [ ] T055 [P] [US3] Create `GoToTimeDialog.qml`, a `Dialog` containing:
  - a `TextField` validated with `PlaybackUtils.parseTime(text, player.duration)`
  - inline error text for `malformed` ("Use h:mm:ss, m:ss or seconds") and `outOfRange` ("Beyond the end of the media")
  - OK, which seeks and closes. It accepts on Enter (FR-023)
- [ ] T056 [US3] Make clicking the elapsed-time label in `MediaControlsComponent.qml` open `GoToTimeDialog`.
- [ ] T057 [US3] Extend `AppConstants.shortcutReference` (added with the Settings redesign in US1; the Shortcuts tab of `SettingsDialog.qml` already repeats over it) so it covers every row of the table in [contracts/keyboard-and-cli.md](./contracts/keyboard-and-cli.md) (FR-025).
- [ ] T058 [US3] Add the remaining FR-024 `Shortcut`s to `Main.qml`, each with `enabled: shortcutsEnabled` (except the text-safe ones) and `showOsd` feedback:
  - Ctrl+Up/Ctrl+Down: volume ±`AppConstants.volumeStep`
  - M: mute toggle via the audio output's `muted`
  - F: full-screen toggle
  - Esc: exit full-screen only when no popup is open
  - `[` / `]` / `=`: previous/next `AppConstants.playbackSpeeds` preset, and reset to 1.0
  - E / Shift+E: `stepFrame(+1/−1)`
  - Shift+N / Shift+P: chapters
  - N / P: playlist next/previous (reusing the `nextTrack`/`previousTrack` handlers)
  - Ctrl+T: go to time (text-safe)
  - Ctrl+O: open file (text-safe)
- [ ] T079 [US3] Make shortcuts, the on-screen indicator and dialogs work from the mini player (FR-033):
  - set `context: Qt.ApplicationShortcut` on every `Shortcut` in `Main.qml` so they fire while `MiniPlayerWindow.qml` has focus
  - add a `showOsd(text)` equivalent to `MiniPlayerWindow.qml` and route `mediaComponent.showOsd` calls to whichever window is visible
  - parent `ResumeDialog` and `GoToTimeDialog` to the visible window
- [ ] T059 [US3] Add `GoToTimeDialog.qml` to `QML_FILES` in `CMakeLists.txt`.

**Checkpoint**: Quickstart Q3.1–Q3.7 pass, Q3.1 included (≥48/50 exact steps). If Q3.1 fails, open a follow-up for a qtmultimedia patch (plan Risks). This ships as its own PR.

---

## Phase 6: User Story 4 - System integration and network streams (Priority: P4)

**Goal**: OS media keys and now-playing on Linux, Windows and macOS, Open URL, live-stream handling, and URL hand-off (FR-027 to FR-031).

**Independent Test**: Quickstart scenarios Q4.1–Q4.10.

### Implementation for User Story 4

- [ ] T060 [US4] Create `mediasession.h`/`mediasession.cpp`, the `MediaSession` QObject (`QML_ELEMENT`, `QML_SINGLETON`) following [contracts/media-session.md](./contracts/media-session.md):
  - properties `playbackStatus`, `title`, `artist`, `album`, `durationMs`, `positionMs`, `artworkUrl`, `canGoNext`, `canGoPrevious`, `canSeek`
  - signals `playRequested`, `pauseRequested`, `playPauseRequested`, `stopRequested`, `nextRequested`, `previousRequested`, `seekRequested(qint64)`
  - a private `Backend` interface with a no-op implementation and a compile-time factory
  - `setArtwork(QImage)`, which writes `<CacheLocation>/nowplaying.png` on a `QtConcurrent` worker and sets `artworkUrl` when the write finishes
  - if back-end setup fails, log a warning and fall back to no-op
- [ ] T061 [P] [US4] Create `mediasession_mpris.cpp` (Linux):
  - `QDBusAbstractAdaptor`s for `org.mpris.MediaPlayer2` and `org.mpris.MediaPlayer2.Player`, registered at `/org/mpris/MediaPlayer2` as `org.mpris.MediaPlayer2.gav`, falling back to `.instance<pid>`
  - every property and method in the contract table, with `PropertiesChanged` for everything except `Position`, and `Seeked` after jumps of more than 1 s
  - `OpenUri` accepts `file`/`http`/`https`
- [ ] T062 [P] [US4] Create `mediasession_windows.cpp` (Windows):
  - C++/WinRT `ISystemMediaTransportControlsInterop::GetForWindow` using the main window's HWND
  - enable buttons per the contract, update `DisplayUpdater` (`Video` or `Music`, title/artist/album, thumbnail from `artworkUrl`) and `PlaybackStatus`
  - `UpdateTimelineProperties` about once a second while playing and after every seek, skipped for live streams
  - marshal `ButtonPressed` and `PlaybackPositionChangeRequested` to the GUI thread via `QMetaObject::invokeMethod`
- [ ] T063 [P] [US4] Create `mediasession_macos.mm` (macOS):
  - `MPRemoteCommandCenter` targets for play, pause, toggle, stop, next, previous and changePlaybackPosition, with `enabled` driven by `canGoNext`/`canGoPrevious`/`canSeek`
  - `MPNowPlayingInfoCenter` `nowPlayingInfo` keys and `playbackState` per the contract
  - handlers dispatch to the main queue and return `CommandFailed` when no media is loaded
- [ ] T064 [US4] Update `CMakeLists.txt`:
  - add `mediasession.h/.cpp` to `qt_add_qml_module`
  - Linux: add `mediasession_mpris.cpp`, then `find_package(Qt6 COMPONENTS DBus)` and link `Qt6::DBus`
  - Windows: add `mediasession_windows.cpp` and link `windowsapp`
  - macOS: add `mediasession_macos.mm`, enable `OBJCXX` and link `-framework MediaPlayer -framework Foundation`
  - `OpenUrlDialog.qml` is also added to `QML_FILES`
- [ ] T065 [US4] Connect `MediaSession` in `Main.qml`:
  - bind its properties to the player state, `QMediaMetaData` (title falling back to the playlist name or URL), playlist position (`canGoNext`/`canGoPrevious`, shuffle-aware if US2 is present) and `isSeekable`
  - call `setArtwork` from `CoverArtImage`/`ThumbnailImage`
  - connect its signals to the same handlers as the Space/N/P shortcuts and to seek
  - ignore commands when no media is loaded
- [ ] T066 [US4] Add stream support to `custommediaplayer.h`/`custommediaplayer.cpp`:
  - `isLive` property: `duration == 0 && !isSeekable` after `LoadedMedia`
  - a load watchdog (`AppConstants.streamLoadTimeout` = 15 000 ms) from `LoadingMedia` that stops the player and emits `errorOccurred`-style `streamError(message)` if `LoadedMedia` hasn't been reached
  - skip probing and sidecar discovery for non-local sources
- [ ] T067 [US4] In `SeekBarComponent.qml` and `MediaControlsComponent.qml`, when `player.isLive`, show a "LIVE" badge in place of the duration and disable seeking, the seek preview and the repeat range (FR-031).
- [ ] T068 [P] [US4] Create `OpenUrlDialog.qml`, a `Dialog` with a URL `TextField` that only accepts `http://` and `https://` schemes (inline error otherwise) and emits `urlAccepted(url)`.
- [ ] T069 [US4] Wire Open URL into `TitleBar.qml` and `Main.qml`:
  - add an "Open URL…" item to `fileMenu`, plus a text-safe Ctrl+N `Shortcut`
  - accepting the dialog calls `openUrls([url])`
  - `getMediaInfo` returns `{name: url or stream title, path: url, type: "stream", icon: ""}` for http(s) URLs
  - `streamError` and `errorOccurred` for streams show in the snackbar within the 15 s budget (SC-010)
- [ ] T070 [US4] Update `main.cpp` and `instancemanager.cpp`:
  - positional arguments and `--source` accept `http://`/`https://` URLs as `QUrl` rather than local paths
  - other schemes print a message on stderr and are dropped, with exit code 2 when nothing playable is left
  - the single-instance hand-off forwards URLs unchanged
  - `--collage` still rejects URLs ([contracts/keyboard-and-cli.md](./contracts/keyboard-and-cli.md))

- [ ] T078 [US4] Package check for US4: build DEB/AppImage (Qt DBus deployed), NSIS (WinRT SMTC works) and DMG (MediaPlayer framework linked) from the US4 branch and run quickstart Q4.1–Q4.3 on the installed builds, fixing gaps in `support/cpack.cmake` (Constitution Principle III).

**Checkpoint**: Quickstart Q4.1–Q4.10 pass on each platform. This ships as its own PR.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Documentation, packaging checks and full validation across stories

- [ ] T071 [P] Update `CLAUDE.md`:
  - Architecture: the `GAV_SUBPROCESS=probe` mode, `MediaProbe`/`ProbeWorker`/`SubtitleRenderer`/`SubtitleOverlay`, `PlaybackHistory`, `PlaylistFiles`, `MediaSession` back-ends, and the new QML files
  - Dependencies: libass, uchardet, the FFmpeg `iconv`/`openssl` features, and FFmpeg being linked into `gav`
  - Custom FFmpeg plugin section: CI no longer uses `--x-no-default-features` on cache hits
- [ ] T072 [P] Update `README.md` Usage: subtitles (sidecar naming, drag and drop), playlists (`.m3u8`), opening URLs (`gav https://…`), and a link to the shortcut list in Settings.
- [ ] T073 Check packaging in `support/cpack.cmake`:
  - Qt DBus is deployed on Linux, and DEB/RPM/AppImage run on a clean Ubuntu 24.04 VM with subtitles rendering (quickstart Q1.11)
  - the NSIS/ZIP install runs SMTC on Windows
  - the DMG app shows Now Playing on macOS
  - fix any missing deploy entries
- [ ] T074 Check the `SubtitleOverlay` render cost against the plan's "under 4 ms at 1080p" target, using `test_subtitlerenderer` timings and a debug-level timing log in `subtitleoverlay.cpp`. Optimise compositing (e.g. a reused buffer, dirty-rect copy) if it's over.
- [ ] T075 Run the full [quickstart.md](./quickstart.md) on Linux, Windows and macOS and record the results in the final PR description, including the SC-007 frame-step count and the SC-011 comparison screenshots.
- [ ] T076 Remove the Sync Impact Report HTML comment from `.specify/memory/constitution.md` before the first PR from this plan merges, as the constitution workflow expects.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: no dependencies
- **Foundational (Phase 2)**: needs Setup, and blocks every story
- **US1 (Phase 3)**: needs Foundational
- **US2 (Phase 4)**: needs Foundational, and doesn't depend on US1
- **US3 (Phase 5)**: needs Foundational. T052–T054 (chapters) also need US1's T028.
- **US4 (Phase 6)**: needs Foundational. T065 uses shuffle-aware next/previous if US2 is present; otherwise it uses sequential.
- **Polish (Phase 7)**: needs whichever stories are shipping

### Within-Story Order

- **US1**: T020/T021 → T022, T024 → T023 → T025/T026 → T028 → T029/T030 → UI tasks T031–T036 → T037. The tests T016–T019 can be written as soon as the units they cover have headers.
- **US2**: T039/T040 → T041/T042 → T043–T049 → T050
- **US3**: T051/T052 → T053–T058 → T059
- **US4**: T060 → T061/T062/T063 → T064 → T065–T070

### Shared-File Hot Spots

Tasks in different stories touch these files, so they shouldn't run at the same time:

- `Main.qml`: T015, T035, T041, T042, T045, T048, T058, T065, T069
- `custommediaplayer.cpp`: T028–T030, T051, T052, T066
- `CMakeLists.txt`: T002, T005, T012, T037, T050, T059, T064
- `MediaControlsComponent.qml`: T032, T047, T054, T056, T067

---

## Parallel Examples

### Foundational

```text
T006 playbackutils.h/.cpp      T008 playbackhistory.h/.cpp      T010 playlistio.h/.cpp
T007 test_playbackutils.cpp    T009 test_playbackhistory.cpp    T011 test_playlistio.cpp
T013 AppConstants.qml
```

### User Story 1

```text
T016 test_subtitlefiles.cpp   T017 test_probeprotocol.cpp   T019 test_subtitlerenderer.cpp
T020 subtitlefiles.h/.cpp     T021 probemessage.h/.cpp      T025 subtitlerenderer.h/.cpp
T031 TrackMenu.qml
```

### User Story 2

```text
T038 test_shuffleorder.cpp   T039 shuffleorder.h/.cpp   T040 ResumeDialog.qml   T043 RecentFilesMenu.qml
```

### User Story 3

```text
T053 SeekBarComponent.qml chapter ticks   T055 GoToTimeDialog.qml
```

### User Story 4

```text
T061 mediasession_mpris.cpp   T062 mediasession_windows.cpp   T063 mediasession_macos.mm   T068 OpenUrlDialog.qml
```

---

## Implementation Strategy

### MVP First (User Story 1)

1. Phase 1 and Phase 2, shipped as the foundation PR. The app's behaviour is unchanged and CI is green on all platforms.
2. Phase 3 (US1), then **stop and validate** with quickstart Q1.x on all three platforms.
3. Ship the US1 PR. This is the MVP: GAV can play subtitled and multi-language content.

### Incremental Delivery

1. Foundation, then US1, then US2, then US3, then US4, each as its own PR on an issue-numbered branch (Constitution: Development Workflow).
2. US2 and US4 can be developed alongside US1. Rebase onto the shared-file hot spots above.
3. Run the Polish phase with the last story PR, or as a separate PR.

---

## Notes

- [P] means different files with no dependency on unfinished tasks.
- Every task names the exact files it touches. The quoted constraints come from data-model.md and the contracts.
- Commit after each task or logical group, and validate at each checkpoint before moving on.
