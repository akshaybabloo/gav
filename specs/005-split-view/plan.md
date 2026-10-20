# Implementation Plan: Split View for up to Four Videos

**Branch**: `205-split-view` (spec directory `specs/005-split-view`) | **Date**: 2026-10-21 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `specs/005-split-view/spec.md`

## Summary

GAV plays up to four videos at once in a grid in one window. One of them is selected: it alone is
heard, it is marked, and every control acts on it. Double-clicking a video makes it fill the area,
a second double-click goes full screen, and the user comes back to the grid with everything where
it was. Videos are put into the split view from the playlist, by the usual ways of opening, and
by dropping files onto it.

**Technical approach** (from [research.md](./research.md))

- One player per tile, all in the main process, as the single video is today. Four 1080p videos
  played together with no dropped frames in a measurement on this machine (R1).
- The grid has four fixed slots, each a `MediaComponent` that exists for the whole session. A
  tile's player lives in its slot for as long as its video is in the grid, so the layout can
  change without restarting anything. With one video, the other three are hidden and idle (R6).
- A new C++ unit, `TileGrid`, holds the slots, their order, the selection, focus, and the rules
  for every gesture, and computes the cells. It has no playback code and is unit-tested (R6, R8).
- The window's one `mediaComponent` becomes `activeMedia`, the selected tile. Handlers that react
  to a player's events move into the tile and use its own playlist entry (R2). Repeat, A-B and
  the end-of-media handling move into the tile too (R3).
- Three RTSP pieces from feature 004 that were per-process by accident are fixed for several
  players: opens go through a shared gate one at a time, so the transport variable and the
  failure reason each belong to one open, and the "needs TCP" memory is shared (R4).
- Sound moves by muting: every tile has its own audio output at the window's volume, muted unless
  selected (R7).
- The playlist's current entry is the selected tile's. A new role marks entries in the grid, and
  next and previous skip them (R9).

## Technical Context

**Language/Version**: C++20 and QML (Qt 6.12).

**Primary Dependencies**: Qt Quick and Qt Multimedia with the FFmpeg backend, as today. Nothing new. For testing only, not shipped: MediaMTX 1.21.2 and the `ffmpeg` command (already used by feature 004).

**Storage**: None new. No split-view state is saved (FR-030).

**Testing**: Google Test through `just test`, also under `LC_ALL=C LANG=C`. [quickstart.md](./quickstart.md) for what needs a screen, with silent headless runs and screenshots where possible.

**Target Platform**: Linux (x64 and arm64), Windows, macOS.

**Project Type**: Desktop application (single CMake project, C++ and QML files at the repository root).

**Performance Goals**: Four 1080p 30 fps videos with less than 1% dropped frames each, and a window that answers within 100 ms (SC-003; measured headless: 0 dropped, 22–30% of one core, about 170 MB per extra player). Selection moves the sound within 0.3 s (SC-002). Four local cameras all playing within about 10 s (SC-005; opens are one at a time, about 1.9 s each).

**Constraints**: A single video must behave exactly as before (FR-031). No permanent controls in the playback view (Constitution I). The interface must stay responsive with four players (Constitution II). RTSP opens across tiles must not mix up transports or failure reasons (FR-027).

**Scale/Scope**: One new C++ unit (`TileGrid`) and one small shared RTSP gate, with tests; changes to `CustomMediaPlayer`, `RtspConnection`, `PlaylistModel`, `PlaylistView` and `PreviewImageProvider`; `Main.qml` rewired from one media component to a grid of four slots; `MediaComponent`, `MediaControlsComponent`, `PlayListComponent`, `TitleBar` and `MiniPlayerWindow` adjusted. Four new user-visible texts. Four test paths in the MediaMTX test configuration.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.* Checked against
constitution v1.3.0.

| Principle | Gate | Before research | After design |
|---|---|---|---|
| I. Simple, Focused Player | Serves playing media; no new network use; nothing recorded beyond today; no permanent clutter in the playback view. | PASS, with one point to resolve: what the split view adds to the playback view. | PASS. The split view is a way of playing media the user chose. It adds no request beyond what each source makes alone, and closing a tile ends that tile's requests (FR-032). Nothing new is remembered; the split view is not restored (FR-030). The ways in and out are a playlist context-menu item and a View menu item. The only addition to the playback view is the selection border, drawn only while a split view is showing, to tell which video is heard; tiles have no buttons of their own (FR-005). |
| II. Responsive UI Through Isolation | Slow work off the UI thread; failures surfaced clearly. | PASS, with one point to resolve: four decoders and the window's responsiveness. | PASS. Decoding and opening stay on Qt's own threads, as for one video; measured headless with four players at a fraction of one core (R1). Playback was never in a subprocess, and the split view adds no batch or probing work. The RTSP gate only orders opens; nothing waits on the UI thread. Each tile's failure is shown with its title and leaves the others playing (R12). |
| III. Cross-Platform Parity | Same behaviour on all platforms; packaging keeps working. | PASS | PASS. No platform-specific code. Hardware decoder limits differ by machine; the quickstart's performance check is run on each platform's package. |
| IV. Pinned, Patchable Dependencies | No new dependency; no reliance on Qt's bundled FFmpeg. | PASS | PASS. No new dependency. The RTSP gate exists because Qt's plugin cannot carry a transport or a failure reason per player; it works the same with the packaged plugin. |
| V. Tested Core Logic | Non-visual logic tested; UI in the quickstart; CI green. | PASS | PASS. `TileGrid` (layout, order, selection, focus, gestures), the RTSP gate, the shared TCP memory, the playlist position role, next and previous skipping, and per-player preview names have unit tests (R13). Everything on screen is in the quickstart. |
| Technology Constraints | Qt Quick, Qt Multimedia with FFmpeg; persistence only in the named places; quiet normal runs. | PASS | PASS. No new persisted state. Any new log lines are verbose-only. |
| Development Workflow | Issue branch `205-split-view`; one usable increment per pull request; docs updated with behaviour. | PASS | PASS. Four steps, each usable (R14); `README.md` and `CLAUDE.md` are updated in the step that changes behaviour. |

No violations; Complexity Tracking is empty.

## Project Structure

### Documentation (this feature)

```text
specs/005-split-view/
├── plan.md              # This file
├── research.md          # Phase 0: measurements and decisions (R1–R15)
├── data-model.md        # Phase 1: TileGrid, tiles, RTSP gate, playlist changes
├── quickstart.md        # Phase 1: validation scenarios
├── contracts/
│   └── split-view.md    # Phase 1: behaviour and the QML interface of TileGrid
├── checklists/
│   └── requirements.md  # Spec quality checklist
└── tasks.md             # Phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
tilegrid.h / tilegrid.cpp              # new: slots, order, selection, focus, gestures, cells
rtspopengate.h / rtspopengate.cpp      # new: one RTSP open at a time across players
rtspconnection.h / .cpp                # shared "needs TCP" memory
custommediaplayer.h / .cpp             # waits for the RTSP gate; per-player preview names
previewimageprovider.h / .cpp          # names and clearing per owner
playlistmodel.h / .cpp                 # TilePositionRole, setTileEntries
playlistview.h / .cpp                  # nextRowExcluding, previousRowExcluding
Main.qml                               # four slots, activeMedia, open/drop/next routing, Esc chain
MediaComponent.qml                     # selection border, per-tile repeat and A-B, per-tile handlers
MediaControlsComponent.qml             # repeat and A-B from the selected tile; stop closes a tile
SeekBarComponent.qml                   # A-B points from the selected tile
PlayListComponent.qml / PlaylistRow.qml  # "Play in split view"; position shown in the row
TitleBar.qml                           # View > End split view
MiniPlayerWindow.qml                   # unavailable in a split view (button state in the control bar)
CMakeLists.txt                         # new sources and tests
tests/test_tilegrid.cpp                # new
tests/test_rtspopengate.cpp            # new
tests/test_rtspconnection.cpp          # shared TCP memory
tests/test_playlistmodel.cpp           # position role
tests/test_playlistview.cpp            # next and previous skipping
tests/test_previewimageprovider.cpp    # new: names and clearing per owner
tests/rtsp/mediamtx.yml, README.md     # paths wall1–wall4 and how to publish to them
README.md, CLAUDE.md                   # the feature, and the new units
```

**Structure Decision**: the existing single-project layout. New C++ units sit at the repository
root beside the others and are added to the `appgav` QML module and to `gav_tests`; QML stays at
the root in the `gavqml` module.

## Delivery

From [research.md R14](./research.md). Each step leaves GAV usable and can be its own pull request
under #205, or all can go on the one branch.

| Step | Content | Done when |
|---|---|---|
| 1 | Groundwork with no visible change: `TileGrid` and tests, the RTSP gate and shared TCP memory with tests, per-player preview names, per-tile repeat and A-B, `activeMedia` and the four slots with only slot 0 used | All tests pass; the single-view checks (QR.1) give the same results |
| 2 | User Stories 1 and 2 | Q1.x, Q2.x, QC.1–QC.3, QC.5 |
| 3 | User Story 3 | Q3.x |
| 4 | User Story 4 | Q4.x, QC.4, full quickstart |

## Decisions made in planning

Two points left open by the clarification session were decided here and have since been written
into the spec (FR-005, FR-011, FR-011a, FR-023, FR-025):

- **Stop while a video fills the area** closes that tile and brings the grid back with the rest.
- **The selection mark** is a 2-pixel border in the theme's accent colour, drawn only in a split
  view. For screen readers, each tile's accessible name is its position and title, and the
  selected tile is reported as selected.

## Complexity Tracking

None.
