# Implementation Plan: Player Feature Roadmap

**Branch**: `001-player-feature-roadmap` (spec directory; work is on `main` until per-story branches are cut) | **Date**: 2026-10-02 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `specs/001-player-feature-roadmap/spec.md`

## Summary

This plan adds four independently shippable capabilities to GAV:

1. Full-fidelity subtitles plus audio and subtitle track selection
2. Playback continuity: resume prompt, recent files, shuffle, M3U playlists, optional session
   restore
3. Precise navigation: frame stepping, chapters, go to time, VLC-style shortcuts
4. OS media-session integration and HTTP(S) streams

**Technical approach** (from [research.md](./research.md))

- **Subtitles** are rendered by libass into a `SubtitleOverlay` QQuickItem, synced to each video
  frame's timestamp.
- **Probing**: subtitle events, chapters and embedded fonts come from a new FFmpeg-based probe
  subprocess (`GAV_SUBPROCESS=probe`) that streams JSON lines. This keeps untrusted demuxing out
  of the UI process.
- **Audio tracks, streams and live detection** use `QMediaPlayer` directly.
- **History** lives in `history.json`, and playlists use M3U, both handled by pure C++ classes
  with unit tests.
- **OS media integration** is a `MediaSession` interface with MPRIS (Linux), SMTC (Windows) and
  MediaPlayer.framework (macOS) back-ends.

## Technical Context

| Item | Details |
|------|---------|
| **Language/Version** | C++20 and QML (Qt 6.12) |
| **Primary Dependencies** | Qt 6.12: Core, Gui, Quick, Qml, Multimedia, Widgets, Concurrent, Network, and DBus (new, Linux only). vcpkg: `ffmpeg` 9.0.1 (`avformat`, `avcodec`, `avutil` now also linked into `gav`; new features `iconv` and `openssl` on Linux/macOS), `libass` 0.17.5 (new), `uchardet` (new), `spdlog`, `fmt`, `gtest`. Platform: C++/WinRT SMTC (Windows), `MediaPlayer.framework` (macOS). |
| **Storage** | Existing QML `Settings` for preferences. `<AppDataLocation>/history.json` for positions and recent files. `<AppDataLocation>/session.m3u8` for the last-session playlist. `<CacheLocation>/nowplaying-<0|1>.png` (alternating, so the URL changes with the cover) for artwork. |
| **Testing** | Google Test via `ctest` (`gav_tests`) for all non-UI logic and probe golden files. Manual scenarios are in [quickstart.md](./quickstart.md). |
| **Target Platform** | Linux x64/arm64 (Ubuntu 24.04 CI; DEB/RPM/TGZ/AppImage), Windows x64 (NSIS/ZIP), macOS (DMG) |
| **Project Type** | Desktop application (single project, sources at the repository root) |
| **Performance Goals** | Subtitles composited for every displayed frame at up to 60 fps with no dropped frames. libass render plus composite under 4 ms at 1080p on CI-class hardware. Track switch under 1 s. Probe sends `media` within 1 s for local files. Media commands handled within 500 ms. |
| **Constraints** | Offline by default (Principle I). The UI thread is never blocked for more than 16 ms by new work. Probe crashes must not affect playback. Linux packages must render subtitles with host fonts. |
| **Scale/Scope** | About 11 new C++ units plus 3 platform back-ends, about 6 new QML components, 33 functional requirements, 4 user stories delivered as 4 PRs on top of a shared foundation |

All items are resolved; there are no NEEDS CLARIFICATION entries.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Gate | Pre-research | Post-design |
|-----------|------|--------------|-------------|
| I. Simple, Focused Player | Every feature serves playback or navigation. Network access only on user action. No accounts or telemetry. Data stays local. No clutter in the main view. | PASS. Network access only through Open URL or a URL argument. History is local and can be cleared. New controls live in menus or popups. | PASS. Contracts add no background network calls. MPRIS and SMTC are local IPC only. |
| II. Responsive UI Through Isolation | Heavy work off the UI thread. Untrusted media processing in a subprocess with a timeout. Failures surfaced. | PASS (provisional). Probing must be isolated. | PASS. Probe subprocess with a 5 s `hello` watchdog ([probe-protocol.md](./contracts/probe-protocol.md)). libass rendering on a worker thread. History written with `QSaveFile` off the UI thread. Probe crashes produce a snackbar and a plain-text fallback. |
| III. Cross-Platform Parity | Builds and tests on all CI targets. Platform code behind one interface with a no-op fallback. Packaging intact. | PASS (provisional). Media sessions differ per OS. | PASS. `MediaSession` has 3 back-ends plus a no-op ([media-session.md](./contracts/media-session.md)). libass font providers per OS. The Linux fontconfig workaround is checked by quickstart Q1.11. |
| IV. Pinned, Patchable Dependencies | Dependencies come from vcpkg with pinned versions. Custom FFmpeg plugin kept. New dependencies justified. | NEEDS JUSTIFICATION. New: libass, uchardet, FFmpeg linked into `gav`, ffmpeg `iconv`/`openssl` features. | PASS with justification. See Complexity Tracking. All come from vcpkg at the pinned `VCPKG_COMMIT`, and the plugin still uses vcpkg FFmpeg. |
| V. Tested Core Logic | Unit tests for non-UI logic. Regression tests for fixes. Manual steps for UI behaviour. Green CI. | PASS (provisional) | PASS. 7 test files are listed in [quickstart.md](./quickstart.md#automated-checks). All UI behaviour has numbered manual scenarios. |
| Technology Constraints | Qt Settings for persisted state unless justified. Version injection. QML at the root. | NEEDS JUSTIFICATION (`history.json`) | PASS with justification. See Complexity Tracking. |
| Development Workflow | One increment per PR. Issue-numbered branches. No comments by default. | PASS. Foundation PR, then one PR per story (P1 to P4). | PASS |

**Gate result**: PASS. Two justified deviations are recorded below. No unjustified violations.

## Project Structure

### Documentation (this feature)

```text
specs/001-player-feature-roadmap/
├── plan.md               # This file
├── research.md           # Phase 0: decisions R1–R14
├── data-model.md         # Phase 1: entities, rules, state machines
├── quickstart.md         # Phase 1: automated and manual validation
├── contracts/
│   ├── probe-protocol.md     # UI ↔ probe subprocess JSON Lines protocol
│   ├── playlist-format.md    # M3U read/write rules
│   ├── keyboard-and-cli.md   # Shortcut table and command-line additions
│   └── media-session.md      # MPRIS / SMTC / MediaPlayer.framework mapping
├── checklists/
│   └── requirements.md
└── tasks.md              # Phase 2 (/speckit-tasks; not created here)
```

### Source Code (repository root)

GAV keeps all sources at the repository root (QML module `gavqml`). New files follow that
convention. Entries are new (`+`) or modified (`~`).

```text
# Foundation (shared by all stories)
+ playbackutils.h/.cpp        # parseTime, chapter navigation target, resume window
+ playbackhistory.h/.cpp      # history.json load/save, eviction, recent list
+ playlistio.h/.cpp           # M3U read/write, PlaylistDocument
~ main.cpp                    # GAV_SUBPROCESS=probe dispatch; URL args; FONTCONFIG_FILE on Linux
~ CMakeLists.txt              # new sources, libass/uchardet/FFmpeg linking, Qt6::DBus on Linux, platform back-ends, tests
~ vcpkg.json                  # libass, uchardet; ffmpeg iconv + openssl (linux/osx)
~ .github/workflows/build.yaml  # stop skipping FFmpeg on plugin-cache hits
~ AppConstants.qml            # subtitle extensions, delay step, scale bounds

# Story 1: Subtitles and tracks
+ mediaprobe.h/.cpp           # parent side: QProcess launch, JSON Lines client, track/chapter model
+ probeworker.h/.cpp          # child side: FFmpeg demux/decode, uchardet, protocol writer
+ probemessage.h/.cpp         # protocol line parse/serialise (shared, unit-tested)
+ subtitlefiles.h/.cpp        # sidecar discovery and preferred-language choice
+ subtitlerenderer.h/.cpp     # libass library/renderer/track wrapper, ASS_Image → QImage
+ subtitleoverlay.h/.cpp      # QQuickItem (QML_ELEMENT) bound to a QVideoSink and SubtitleRenderer
+ subtitlecontroller.h/.cpp   # track list, selection, delay, size, chapters; owns MediaProbe and SubtitleRenderer
+ TrackMenu.qml               # subtitle/audio selection, delay, size, load file…
~ custommediaplayer.h/.cpp    # audio/subtitle track lists, active track, probe ownership, chapters
~ MediaComponent.qml          # SubtitleOverlay above VideoOutput; drop .srt/.ass/.vtt
~ MiniPlayerWindow.qml        # SubtitleOverlay for the mini output
~ MediaControlsComponent.qml  # track menu button

# Story 2: Playback continuity
+ shuffleorder.h/.cpp         # Fisher–Yates remaining-order logic
+ ResumeDialog.qml            # "Resume from …?" Resume / Start over
+ RecentFilesMenu.qml
~ PlayListComponent.qml       # shuffle toggle, save/open playlist, session restore
~ SettingsDialog.qml          # remember positions, restore last playlist, clear history, subtitle prefs
~ Main.qml                    # Settings properties, history wiring, startup restore order

# Story 3: Precise navigation
+ GoToTimeDialog.qml
~ SeekBarComponent.qml        # chapter markers and hover titles
~ custommediaplayer.h/.cpp    # stepFrame(±1), nextChapter/previousChapter
~ Main.qml                    # FR-024 shortcuts gated by shortcutsEnabled
~ SettingsDialog.qml          # shortcut reference generated from one list

# Story 4: System integration and streams
+ mediasession.h/.cpp         # abstract interface, factory, no-op back-end
+ mediasession_mpris.cpp      # Linux (Qt6::DBus)
+ mediasession_windows.cpp    # Windows (C++/WinRT SMTC)
+ mediasession_macos.mm       # macOS (MediaPlayer.framework)
+ OpenUrlDialog.qml
~ instancemanager.cpp         # forward http(s) URLs
~ custommediaplayer.h/.cpp    # isLive, network timeout (10 s), load watchdog (120 s)

tests/
+ test_playbackutils.cpp
+ test_playbackhistory.cpp
+ test_playlistio.cpp
+ test_shuffleorder.cpp
+ test_subtitlefiles.cpp
+ test_probeprotocol.cpp
+ test_subtitlerenderer.cpp
+ data/                       # fixtures listed in quickstart.md
```

**Structure decision**: Keep the existing flat, single-project layout. Each new unit is one
`.h/.cpp` pair registered in `qt_add_qml_module(... SOURCES ...)`. Units without a GUI (`playbackutils`,
`playbackhistory`, `playlistio`, `shuffleorder`, `subtitlefiles`, `probemessage`, `subtitlerenderer`)
are also compiled into `gav_tests`, following how `collage.cpp` is shared today.

## Delivery Order

The order matches the spec's priorities. Each step is its own PR (Constitution: Development
Workflow).

| Step | Contents | Depends on |
|------|----------|------------|
| 0. Foundation | vcpkg/CMake/CI changes, `playbackutils`, `playbackhistory`, `playlistio` and their tests | — |
| 1. Story 1 | Probe subprocess, libass renderer and overlay, track menu | 0 |
| 2. Story 2 | Resume prompt, recent files, shuffle, playlist save/open, session restore | 0 |
| 3. Story 3 | Frame stepping, chapters (reuses the probe from Story 1), go to time, shortcuts | 0, and 1 for chapters |
| 4. Story 4 | Media sessions, Open URL, live detection, URL hand-off | 0 |

Stories 2 and 4 can run in parallel with Story 1. Story 3's chapter work needs the probe from
Story 1. Frame stepping, go to time and the shortcuts do not.

## Risks

| Risk | Mitigation |
|------|------------|
| Frame stepping accuracy below SC-007 on some files (R6) | Measured by quickstart Q3.1. If needed, a follow-up task adds a qtmultimedia patch next to the existing ones in `support/` |
| Static fontconfig on Linux can't find its config (R2) | `FONTCONFIG_FILE` fallback at startup, with AppImage check Q1.11 |
| CI time grows because FFmpeg is always installed (R3) | The vcpkg binary cache already stores FFmpeg. Only cold caches pay the full cost |
| `ffmpeg[openssl]` changes the plugin cache key (R11) | Expected one-off cold build. The scheduled `main` run warms caches for PRs |
| Large font attachments inflate probe output | Fonts go to a temporary directory and are sent as paths, never inline ([probe-protocol.md](./contracts/probe-protocol.md)) |

## Complexity Tracking

> These deviations are recorded because the Constitution Check flagged them.

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| New dependency: **libass** (+ freetype, fribidi, harfbuzz, and fontconfig on Linux) | FR-001a/SC-011 need full ASS fidelity (clarified 2026-10-02). libass is the reference renderer. | Qt's `subtitleText` is plain text only. An FFmpeg subtitle filter in the plugin can't change delay or size live and is missing from Qt's stock plugin. Writing our own renderer would be enormous and wouldn't match reference players. |
| **FFmpeg linked into `gav`** (used only in the probe subprocess) | Chapters, embedded ASS headers and events, font attachments and external subtitle decoding aren't exposed by Qt Multimedia. | Patching qtmultimedia wouldn't work with the stock plugin used in local development. A second helper binary adds packaging work for no isolation benefit over re-running `gav`. |
| New dependency: **uchardet** + `ffmpeg[iconv]` | Non-UTF-8 subtitle files are common, and the spec's edge cases need them displayed correctly. | Qt's `QStringDecoder` depends on platform ICU availability. A fixed Windows-1252 fallback garbles non-Latin scripts. |
| `ffmpeg[openssl]` on Linux/macOS | FR-029 needs HTTPS streams. vcpkg's FFmpeg has no TLS on these platforms without it. | GnuTLS has a heavier dependency tree. Secure Transport is deprecated by Apple and only covers macOS. |
| **history.json** instead of Qt Settings | Up to 200 position records rewritten on every save. Keeping them separate keeps the settings file small and lets writes be atomic and asynchronous. | QSettings arrays rewrite the whole settings file and mix volatile history with preferences. SQLite would be a new dependency for 200 rows. |
