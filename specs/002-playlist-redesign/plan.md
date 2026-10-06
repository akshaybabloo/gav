# Implementation Plan: Playlist Redesign

**Branch**: `002-playlist-redesign` (spec directory; per-story branches are cut from GitHub issues) | **Date**: 2026-10-05 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `specs/002-playlist-redesign/spec.md`

## Summary

The playlist becomes something the user can keep open while watching, read at a glance, search
when it holds thousands of channels, and edit several entries at a time.

1. **Browse while watching**: with a video showing, the playlist is a resizable panel over the
   right side of the picture instead of a list that covers the window.
2. **Informative rows**: each entry shows its kind, duration or "Live", group and availability,
   and optionally its channel logo.
3. **Finding**: search on title and group, filters, sort orders and collapsible groups, all of
   which stay responsive at 10,000 entries.
4. **Managing**: multi-select, remove with undo, drag reordering, "play next", insert on drop,
   remove duplicates, and a full keyboard map.

**Technical approach** (from [research.md](./research.md))

- The playlist moves out of a QML `ListModel` into two C++ models: `PlaylistModel` (the entries,
  the current entry, the play-next queue, undo) and `PlaylistView` (search, filter, sort, grouping,
  selection, and what "next" means). Both are unit-tested.
- The `ListView` shows `PlaylistView`'s flat rows with fixed-height, reused delegates, which is
  what removes today's per-row cost on large lists.
- The M3U reader and writer keep each entry's group, logo address and raw attribute text, so IPTV
  playlists survive a save.
- Channel logos come through an image provider with scheme, size and time limits and a small
  hashed cache, behind a setting that is on by default.
- `ShuffleOrder` switches from row indices to entry ids so it can follow the filtered view.
- Delivery is in six pull requests so the app works after each: the model swap with no visible
  change, then one per user story, with logos as their own step after a constitution amendment.

## Technical Context

**Language/Version**: C++20 and QML (Qt 6.12)

**Primary Dependencies**: Qt 6.12 Core, Gui, Quick, Qml, Concurrent, Network (all already linked). No new vcpkg or Qt dependencies. WebP logos rely on the `qtimageformats` plugin CI already installs; without it they fall back to the kind icon.

**Storage**: Entries in memory, saved as M3U ([format](./contracts/playlist-format.md)). Three new keys in the existing QML `Settings`. Logo cache in `<CacheLocation>/logos/`.

**Testing**: Google Test via `ctest` (`just test`) for `PlaylistModel`, `PlaylistView`, `PlaylistIO` and `ShuffleOrder`. Manual scenarios in [quickstart.md](./quickstart.md).

**Target Platform**: Linux x64/arm64, Windows x64, macOS (unchanged)

**Project Type**: Desktop application (single CMake project, QML files at the repository root)

**Performance Goals**: With 10,000 entries: first rows within 2 s of the playlist being read (SC-004); each search keystroke, sort or group change applied within 200 ms (SC-003); no UI stall longer than that (FR-028).

**Constraints**: No network request from the playlist unless the logo setting is on, which it is by default (FR-029). Logo requests limited to `http`/`https`, 10 s, 512 KB, 4 in flight. Offline behaviour unchanged. The mini player is untouched.

**Scale/Scope**: Playlists up to tens of thousands of entries. About 2 new C++ model classes, 1 image provider, 3 new QML files, and changes to `Main.qml`, `PlayListComponent.qml`, `playlistio`, `shuffleorder`, `SettingsDialog.qml` and `AppConstants.qml`.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.* Checked against
constitution v1.3.0.

| Principle | Gate | Before research | After design |
|---|---|---|---|
| I. Simple, Focused Player | Serves playing and navigating. Offline by default. Network only on user action, with limits. Nothing recording playback unless opted in. Choices the media offers stay selectable. No permanent clutter. | PASS, with one point to resolve: channel logos need network access that is not "needed to play". | PASS. Constitution v1.3.0 allows download of decorative playlist images behind a setting that may be on by default, under conditions this design meets: the setting can be turned off in Settings and nothing is requested while it is off, logos are requested only for rows in view, cancelled when the setting is turned off, limited, cached as scaled images under a hash, and cleared with history. The panel is closed by default while a video shows. The play queue is not saved. |
| II. Responsive UI Through Isolation | Slow work off the UI thread. Untrusted media processing in a subprocess. Network content cannot reach local files. Failures surfaced. | PASS (provisional): view rebuild cost unknown. | PASS. Rebuild is measured and moves to a worker if above 50 ms (R3). Logos are decoded on a worker thread, in-process, which the constitution allows (since v1.2.0) for small still images with a format allow-list and input-size and pixel limits (R6). Logo addresses are restricted to `http`/`https`. No file-system check runs on the UI thread before playing; a vanished file is reported by the player's error (R9). Unavailable entries show their reason. |
| III. Cross-Platform Parity | Same behaviour everywhere. Platform code behind one interface. Packaging keeps working. | PASS | PASS. Only "Show in file manager" is platform-specific, behind `revealInFileManager` with a fallback (R12). No packaging change. |
| IV. Pinned, Patchable Dependencies | No unpinned or unjustified dependency. | PASS | PASS. No new dependency. |
| V. Tested Core Logic | Non-visual logic has unit tests. Bug fixes get regression tests. UI has quickstart steps. | PASS | PASS. All playlist logic moves into tested C++ classes; today it is untested QML. |
| Technology Constraints | QML at the repository root in `gavqml`. Persistence through Qt Settings, `AppDataLocation` or `CacheLocation`. Quiet normal runs. | PASS | PASS. |
| Development Workflow | One usable increment per PR. Spec documents kept in step. | PASS | PASS. Six PRs (see Delivery order). The change to how missing playlist entries are handled updates the 001 contract and quickstart in the same PR. |

**Result**: Gates pass with no deviations. No unresolved clarifications remain.

## Project Structure

### Documentation (this feature)

```text
specs/002-playlist-redesign/
├── plan.md              # This file
├── research.md          # Phase 0: decisions R1–R12
├── data-model.md        # Phase 1: entry, model, view, settings, logo cache
├── quickstart.md        # Phase 1: automated and manual validation
├── contracts/
│   ├── playlist-model.md    # QML-facing API of PlaylistModel, PlaylistView, ShuffleOrder, LogoProvider
│   ├── playlist-format.md   # M3U extensions: groups, logos, attribute round trip
│   └── playlist-panel.md    # Where the playlist appears, mouse, menu and keyboard behaviour
├── checklists/
│   └── requirements.md
└── tasks.md             # Phase 2 output (/speckit-tasks; not created by /speckit-plan)
```

### Source Code (repository root)

```text
+ playlistmodel.h/.cpp        # Entries, current entry, queue, remove with undo, move, duplicates
+ playlistview.h/.cpp         # Search, filter, sort, grouping, selection, next/previous
+ logoprovider.h/.cpp         # Opt-in channel logo image provider and cache
+ PlaylistPanel.qml           # Panel chrome: header, search row, resize edge, empty states
+ PlaylistRow.qml             # Entry row delegate
+ PlaylistGroupHeader.qml     # Group header delegate
~ PlayListComponent.qml       # Becomes the list itself, used by the panel and the full-area view
~ Main.qml                    # Uses PlaylistModel/PlaylistView; panel placement; Ctrl+L, Ctrl+F; Esc order
~ playlistio.h/.cpp           # group, logo, attributes, #EXTGRP; missing files kept as unavailable
~ shuffleorder.h/.cpp         # Entry ids and setCandidates
~ playbackutils.h/.cpp        # revealInFileManager
~ SettingsDialog.qml          # "Show channel logos"; Clear history also clears the logo cache
~ AppConstants.qml            # Panel sizes, new shortcuts in the reference list
~ MediaControlsComponent.qml  # Playlist button reflects panel state
~ CMakeLists.txt              # New sources and QML files

tests/
+ test_playlistmodel.cpp
+ test_playlistview.cpp
~ test_playlistio.cpp         # Attributes, #EXTGRP, unavailable entries, round trip
~ test_shuffleorder.cpp       # Id-based
+ data/mixed.m3u8, data/tone.mp3, data/generate-playlist.sh
```

`+` new, `~` changed.

**Structure Decision**: Single project, flat layout, matching the rest of GAV: C++ sources and QML
files at the repository root, tests in `tests/`. Playlist logic that was in `Main.qml` and
`PlayListComponent.qml` moves into `playlistmodel` and `playlistview` so it can be tested.

## Delivery order

Each step is its own issue, branch and PR, and leaves the app working. The steps match the phases
in [tasks.md](./tasks.md). The tracking issue is #191, and polish and validation is #190. Branches
are named `<issue-number>-<short-slug>`.

| Step | Content | Spec coverage | Visible change |
|---|---|---|---|
| 1. Model swap (Foundational), #184 | `PlaylistModel` replaces the `ListModel`; current entry moves into the model; pass-through `PlaylistView`; `ShuffleOrder` uses ids | FR-026 | None |
| 2. Panel (Story 1), #185 | `PlaylistPanel`, placement over the video, toggles, remembered state, empty state | FR-001 to FR-005, FR-008 | Panel over video |
| 3. Rows (Story 2, without logos), #186 | Playlist-file attributes, availability, lazy duration and live state, `PlaylistRow` | FR-006 to FR-011, FR-013, FR-027 | Two-line rows |
| 4. Logos (Story 2, after the constitution amendment), #187 | `LogoProvider`, its cache and its setting | FR-012, FR-012a, FR-012b, FR-029 | Optional logos |
| 5. Finding (Story 3), #188 | Search, filters, sort, grouping, view-following next/previous and shuffle, timing test | FR-014 to FR-019, FR-028 | Search row, groups |
| 6. Managing (Story 4), #189 | Selection, remove with undo, reorder, row menu, play next, insert on drop, duplicates, keyboard map | FR-020 to FR-025 | Editing |

## Complexity Tracking

No violations. The constitution was amended twice for channel logos, both on 2026-10-06:

- Under v1.1.0 two logo items were deviations: downloading images that are not needed to play the
  media (Principle I) and decoding them in the main process (Principle II). v1.2.0 permits both
  under conditions, one of which was that the setting is off by default.
- After trying the feature the user chose to have "Show channel logos" on by default. v1.3.0
  allows the setting to be on by default, provided it can be turned off in Settings and nothing is
  requested while it is off.

The design meets the conditions of v1.3.0: a setting that can be turned off, rows in view only,
`http`/`https`, 10 s and 512 KB limits, cancelled when turned off, a clearable cache, and decoding
off the UI thread restricted to PNG, JPEG and WebP up to 1024 × 1024 pixels.
