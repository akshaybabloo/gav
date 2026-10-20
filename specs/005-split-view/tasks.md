---

description: "Task list for Split View for up to Four Videos"
---

# Tasks: Split View for up to Four Videos

**Input**: Design documents from `specs/005-split-view/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/split-view.md](./contracts/split-view.md), [quickstart.md](./quickstart.md)

**Tests**: Included. The constitution requires unit tests for logic that runs without a GUI, and
the plan puts the grid's rules (`TileGrid`) and the RTSP open order (`RtspOpenGate`) in units with
no playback code for that reason. What needs a screen is checked with
[quickstart.md](./quickstart.md), using silent headless runs and screenshots where possible.

**Issue**: #205 · **Branch**: `205-split-view`

**Organization**: Tasks are grouped by user story in the spec's priority order, after a
foundational phase that changes nothing visible. The plan's four delivery steps follow the same
order (see [Delivery order](#delivery-order)).

**Review checklist**: [checklists/review.md](./checklists/review.md) is unreviewed (0 of 36).
`/speckit-implement` asks before starting while it has unchecked items.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on unfinished tasks)
- **[Story]**: The user story the task belongs to (US1–US4)

## Path Conventions

GAV keeps every source file at the repository root. QML files belong to the `gavqml` module, and
C++ units are registered in `qt_add_qml_module(appgav … SOURCES …)` in `CMakeLists.txt`. Tests
live in `tests/` and are compiled into the `gav_tests` target, which lists the units it needs
again. Build, test, format and lint only through `just`, which uses `build-cli/`. Run the tests
under `LC_ALL=C LANG=C` as well. Headless runs of the application are silent. Icons in QML stay
as `"\uXXXX"` escapes. Default to no code comments. In this file, "the window" is `Main.qml` and
"a tile" is a `MediaComponent` in one of the four slots.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Test sources for the camera checks

- [ ] T001 [P] Add the open paths `wall1`, `wall2`, `wall3` and `wall4` to `tests/rtsp/mediamtx.yml` (readable by the user `any`, like `test`), and add to `tests/rtsp/README.md` the `ffmpeg` command that publishes a test picture with the time drawn on it (`drawtext` with `%{localtime}`) to each of them, so that quickstart QC.1–QC.4 and Q3.7 can tell a live picture from a paused one.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Step**: 1, Groundwork with no visible change

**Purpose**: The grid's rules, RTSP that works with several players, per-tile state, and the
window rewired to a selected tile, with only one tile ever used

**⚠️ CRITICAL**: No story can start until this phase is complete. At the end of it GAV behaves
exactly as before (FR-031).

### Tests for the foundation

- [ ] T002 [P] Write `tests/test_tilegrid.cpp` against [data-model.md → TileGrid](./data-model.md#tilegrid-new-c-exposed-to-qml) and [research.md R8](./research.md): `open` with `count` 0 puts the entry in slot 0 (count 1, selected) and with `count` 1 replaces slot 0's entry; `add` after `open` keeps slot 0 at position 1 and selected; `start` needs 2–4 entries and fills slots 0… with position 1 selected; `add` leaves out entries already in a slot, takes the next free slots and keeps the selection, and is refused as a whole beyond 4 ("A split view MUST hold at most four videos"); `replace` of an entry already in another slot selects that slot instead; `close` removes the slot from the order, keeps the others' relative order, selects position 1 and leaves `Focused`; `end` keeps only the selected slot; an entry id appears in at most one slot; `cellFor` for 1 (whole area), 2 (left and right halves), 3 and 4 (2×2, fourth cell empty with 3) at two area sizes, the whole area for the focused slot and an empty rectangle for the others in `Focused`; `slotAt` returns −1 over the empty cell; `doubleClick`, `escape` and `pressPosition` for every row of the R8 table with and without full screen; `pressPosition` for a position with no video does nothing; `pauseForFocus` / `takeResumeAfterFocus` hand back only the slots recorded and clear them; `entryRemoved` closes the slot.
- [ ] T003 [P] Write `tests/test_rtspopengate.cpp`: the first request is admitted at once; a second waits until the first is released; waiting requests are admitted in arrival order; cancelling a waiting request removes it; releasing or cancelling the holder admits the next; releasing an id that does not hold the gate changes nothing; `beginOtherOpen` / `endOtherOpen` count non-RTSP opens, and the holder is marked `overlapped` when that count is above 0 at any time during its open (including an open that began before it was admitted), and not otherwise; a new holder starts not overlapped.
- [ ] T004 [P] Extend `tests/test_rtspconnection.cpp`: a source learned as needing TCP by one `RtspConnection` is opened over TCP first by a second one; `forgetTransports` on either clears it for both; each test starts from an empty memory.
- [ ] T005 [P] Write `tests/test_previewimageprovider.cpp`: images stored by two owners do not collide when stored in the same millisecond; `clearImages(owner)` removes that owner's images only.

### Implementation of the foundation

- [ ] T006 Create `tilegrid.h` / `tilegrid.cpp` (`QML_ELEMENT`, `QObject`) per [data-model.md](./data-model.md) and the "TileGrid, as QML sees it" table in [contracts/split-view.md](./contracts/split-view.md): properties `count`, `splitView`, `selectedSlot`, `focused`, `focusedSlot`, `entryIds`; functions `positionOf`, `cellFor`, `slotAt`, `canAdd`, `open`, `start`, `add`, `replace`, `close`, `end`, `select`, `pressPosition`, `doubleClick`, `escape`, `pauseForFocus`, `takeResumeAfterFocus`, `entryRemoved`; `doubleClick` and `escape` return a map `{ handled, enterFullScreen, leaveFullScreen }`; signal `changed()`. Register it in `CMakeLists.txt` (the `appgav` module and `gav_tests`, with `tests/test_tilegrid.cpp`). T002 passes.
- [ ] T007 Create `rtspopengate.h` / `rtspopengate.cpp`: one instance for the process with `request(id)` (returns whether admitted now), `release(id)`, `cancel(id)`, `beginOtherOpen()`, `endOtherOpen()`, `overlapped(id)` and a signal `admitted(id)`; register in `CMakeLists.txt` with `tests/test_rtspopengate.cpp`. T003 passes.
- [ ] T008 Move the "needs TCP" set out of `RtspConnection` (`m_needsTcp` in `rtspconnection.h`) into one store shared by all connections in `rtspconnection.cpp`, read and written by `open` and the attempt outcomes as now; `forgetTransports` clears the shared store; add a test-only reset. T004 passes.
- [ ] T009 Make `CustomMediaPlayer` (`custommediaplayer.h` / `.cpp`) take the RTSP gate: `openRtspAttempt` asks `RtspOpenGate` with the player's id and, if not admitted, waits for `admitted`; only once admitted does it call `takeOpenFailure`, set `QT_FFMPEG_RTSP_TRANSPORT`, the network timeout and the source. Release the gate when Qt reports the source loaded (`LoadedMedia` or later) or an error, and cancel on `closeRtsp`, `stop` and a new source. The failure note is read only by the holder, and when the gate reports the holder `overlapped` a noted failure is reported as "other". For every non-RTSP source, the player calls `beginOtherOpen` when it sets the source and `endOtherOpen` when Qt reports it loaded or failed, or the source is replaced or stopped. Single-player behaviour and every existing test unchanged.
- [ ] T010 [P] Give `PreviewImageProvider` (`previewimageprovider.h` / `.cpp`) owner-prefixed image names and `clearImages(owner)`; `CustomMediaPlayer` passes its own id when storing and in `resetPreviewPlayer`. Add `previewimageprovider.cpp` and `tests/test_previewimageprovider.cpp` to `gav_tests`. T005 passes.
- [ ] T011 Move repeat mode (0 none, 1 once, 2 loop, 3 A-B), the A-B points and the end-of-media handling from `MediaControlsComponent.qml` (its `Connections` on `player`: `onMediaStatusChanged`, `onPositionChanged`, `onDurationChanged`) and `mainWindow.repeatMode` into `MediaComponent.qml`, so each tile repeats on its own. When a video ends with no repeat, `MediaComponent` emits `endedWithoutRepeat()` instead of calling `nextTrack()` itself.
- [ ] T012 Make `MediaControlsComponent.qml` and `SeekBarComponent.qml` read and change repeat mode and A-B points on the bound tile (a new `media` property set to the window's `activeMedia`), and keep the fast-forward and fast-rewind state reset on end of media.
- [ ] T013 Give `MediaComponent.qml` the per-tile properties `slot` and `entryId`, and turn the window's per-player reactions into tile signals carrying `entryId`: loaded (window then calls `playList.setLoaded`, records the recent file, applies `appSettings.playbackRate`, autoplays, and runs the resume prompt and pending subtitle files only when the tile is the only one), error, `rtspFailed`, `positionCheckpoint`, subtitle error. Each tile binds `rtspTransport` to `appSettings.rtspTransport`. The window's handlers in `Main.qml` (the `Connections` at "target: mediaComponent.mediaPlayer", "target: mediaComponent", "target: mediaComponent.mediaPlayer.subtitles") use the tile's `entryId` instead of `playList.currentId`.
- [ ] T014 Replace the single `MediaComponent { id: mediaComponent }` in `Main.qml` with a `TileGrid { id: tileGrid }` and four `Loader`s (slots 0–3), each active only while `tileGrid.entryIds[slot] >= 0` (slot 0 also while a single video plays as today), placed with `tileGrid.cellFor(slot, width, height)`; add `readonly property var activeMedia` bound to the selected slot's item and change every use of `mediaComponent` that acts on "the player" to `activeMedia` (control bar, both control-bar loaders, track and quality menus, statistics overlay, mini player, `MediaSession`, volume and mute, screenshot, go to time, chapters, window title). Every single-view open (`openAndPlay`, `openUrls`, the file dialog, `selectPlaylistItem`) calls `tileGrid.open(entryId)`. `onAboutToQuit` checkpoints every loaded tile. The end-of-media `endedWithoutRepeat` of the only tile moves to the next entry as `nextTrack()` did.
- [ ] T015 Run `just test`, `LC_ALL=C LANG=C just test` and `just lint` (no findings on added lines), then silent headless runs of the single view: play a file, next and previous, repeat once, loop and A-B, the resume prompt, a stream with qualities, an RTSP stream from `tests/rtsp/`, the mini player. Results must match the earlier features' quickstarts (QR.1).

**Checkpoint**: Nothing visible has changed; every test passes.

---

## Phase 3: User Story 1 - Watch up to four videos at once (Priority: P1) 🎯 MVP

**Step**: 2 (with Phase 4)

**Goal**: Two to four playlist entries play together in a grid; Stop closes a tile; the split view
can be ended.

**Independent Test**: Select four local clips, "Play in split view", and see all four play in a
2×2 grid with only the top-left one heard (quickstart Q1.1–Q1.8).

### Tests for User Story 1

- [ ] T016 [P] [US1] Extend `tests/test_playlistmodel.cpp`: `setTileEntries(ids)` sets `tilePosition` to 1–4 in the given order and 0 for every other entry, emits `dataChanged` only for rows whose value changed, and survives removal and reordering of entries.

### Implementation for User Story 1

- [ ] T017 [US1] Add `TilePositionRole` (`"tilePosition"`) and `Q_INVOKABLE void setTileEntries(const QList<int> &ids)` to `playlistmodel.h` / `.cpp`; include it in `toMap`. T016 passes.
- [ ] T018 [US1] Add "Play in split view" to the context menu in `PlayListComponent.qml`, acting on the selection (`playlistView.selectedIds()`) or the clicked row: enabled when the entries not already in a tile, plus the videos already playing, make 2–4 (`tileGrid.canAdd`); when it would be more than four, disabled with the tooltip "A split view holds up to four videos"; disabled for a single entry with nothing playing.
- [ ] T019 [US1] Add `function playInSplitView(ids)` to `Main.qml`: with nothing playing, `tileGrid.start(ids)`; with one video playing, keep it in slot 0 (no reload, still selected) and `tileGrid.add(ids)`; in a split view, `tileGrid.add(ids)`; load each new slot's entry, starting muted and playing; call `playList.setTileEntries` whenever `tileGrid` changes; leave the playlist panel open or closed as it was (FR-003a).
- [ ] T020 [P] [US1] Show the position from `tilePosition` in `PlaylistRow.qml` (a small "1"–"4" before the title, only when not 0) without changing the row's height.
- [ ] T021 [US1] Make Stop close the selected tile in a split view: the control bar's Stop button in `MediaControlsComponent.qml` and the window's stop paths in `Main.qml` (`onStopRequested` from `MediaSession`) call `tileGrid.close(tileGrid.selectedSlot)`, stop and unload that slot, and update `setTileEntries`; with one tile left it is the single view and Stop behaves as before (FR-005).
- [ ] T022 [P] [US1] Add "End split view" to the View menu in `TitleBar.qml`, visible only while `tileGrid.splitView`, calling a window function that runs `tileGrid.end()` and stops and unloads the other slots.
- [ ] T023 [US1] Keep a tile whose video ended, failed or was lost in its place in a split view: the window ignores `endedWithoutRepeat` when `tileGrid.splitView` (FR-024, FR-024a), and a tile's failure does not stop or pause the others.
- [ ] T024 [US1] Make failure messages name the video in a split view: the playback error dialog and the stream snackbar in `Main.qml` start with the entry's title from its display form ("Camera 2: The stream was lost."); the entry marked unavailable is the tile's (FR-025).
- [ ] T025 [US1] Close tiles whose entries leave the playlist: on removal and on clear, call `tileGrid.entryRemoved(id)` for each removed id and unload those slots (spec edge cases).
- [ ] T026 [US1] Make the mini player button in `MediaControlsComponent.qml` disabled while `tileGrid.splitView` (tooltip "Not available in a split view"), and guard `onMiniPlayerRequested` in `Main.qml` (FR-029).
- [ ] T027 [US1] Run quickstart Q1.1–Q1.7 headless with screenshots, and record Q1.8 (10 minutes, statistics) for a desktop run in the verification section of this file.

**Checkpoint**: A split view of up to four videos plays; with one tile left everything is as before.

---

## Phase 4: User Story 2 - Choose which video is heard and controlled (Priority: P1)

**Step**: 2 (with Phase 3)

**Goal**: One selected tile, marked, heard and controlled; everything else silent and unchanged.

**Independent Test**: In a split view of four, click each tile and press 1–4; the sound and the
border follow, and every control changes only the selected tile (quickstart Q2.1–Q2.9).

### Implementation for User Story 2

- [ ] T028 [US2] Select a tile on click: `MediaComponent.qml` emits `pressed()` from its `MouseArea` (`onPressed`, before any zoom drag) and the window calls `tileGrid.select(slot)`; clicks on the playlist panel over a tile do not reach it.
- [ ] T029 [US2] Add Shortcuts 1, 2, 3 and 4 in `Main.qml`, enabled only while `tileGrid.splitView` and `mainWindow.shortcutsEnabled`, calling `tileGrid.pressPosition(p, fullScreen)`; a position with no video does nothing (FR-010).
- [ ] T030 [US2] Move the sound with the selection: each tile's `AudioOutput.volume` is the window's volume and `muted` is `!selected || windowMuted`; the volume and mute controls and `changeVolume` in `Main.qml` change the window's values (`appSettings.volume` and a window mute property) rather than one tile's output (FR-009, FR-012).
- [ ] T031 [US2] Draw the selection mark in `MediaComponent.qml`: a 2-pixel border in `Material.accent` around the tile, only while `tileGrid.splitView` and the tile is selected; set each tile's `Accessible.name` to its position and title and `Accessible.selected` to whether it is selected (FR-011, FR-011a).
- [ ] T032 [US2] Keep the playlist's current entry on the selected tile without reloading: when `tileGrid.selectedSlot` changes set `playList.currentRow` to its entry, and make the window's current-entry handler skip loading when that entry is already in a tile (research R9).
- [ ] T033 [US2] Confirm the window title, `MediaSession` now-playing, the seek bar, the statistics overlay, the track, subtitle and quality menus, the speed menu, zoom, brightness and contrast, screenshot and loading a subtitle file all follow `activeMedia` after T014, and fix any that still read a fixed tile; the control bar shows the selected tile's repeat, A-B and speed (FR-013 to FR-016). Confirm each tile keeps its own buffering, "Reconnecting…" and failure indicators whether or not it is selected (FR-026).
- [ ] T034 [US2] Run quickstart Q2.1–Q2.9 headless with screenshots (the selection border in light and dark themes).

**Checkpoint**: Step 2 is complete: User Stories 1 and 2 work together.

---

## Phase 5: User Story 3 - Focus one video, then full screen (Priority: P2)

**Step**: 3

**Goal**: Double-click fills the area, double-click again goes full screen, and the grid comes back
as it was.

**Independent Test**: Quickstart Q3.1–Q3.9.

### Implementation for User Story 3

- [ ] T035 [US3] Route double-click in `MediaComponent.qml`: in a split view, the window calls `tileGrid.doubleClick(slot, fullScreen)` and applies the returned full-screen change; outside a split view, `fullscreenToggleRequested()` as today (FR-019, FR-022).
- [ ] T036 [US3] Pause and resume around focus in `Main.qml`: on entering `Focused`, pause every other tile that is playing and not `isLive`, recording it with `tileGrid.pauseForFocus(slot)`; live tiles keep playing, hidden; on returning to the grid, play the slots from `tileGrid.takeResumeAfterFocus()` only (FR-017, FR-018, FR-021).
- [ ] T037 [US3] Make 1–4 while focused move the focus to that position: the replaced tile is paused if it is a file (and recorded), the new one plays on from where it paused, selected and heard; full screen unchanged (FR-010).
- [ ] T038 [US3] Insert the split view into the Esc chain in `Main.qml`: after clearing the search and closing the playlist overlay, call `tileGrid.escape(fullScreen)`; only if it is not handled, leave full screen as today (FR-020).
- [ ] T039 [US3] Make F (full screen) follow the contract: in the grid it toggles the grid's full screen; while focused it toggles full screen and keeps the focus (FR-023).
- [ ] T040 [US3] Make Stop while focused close the focused tile and return to the grid (`tileGrid.close` already leaves `Focused`); resume the tiles the focus paused (FR-005).
- [ ] T041 [US3] Run quickstart Q3.1–Q3.9 headless with screenshots; for Q3.7, publish to `wall1`–`wall4` (T001).

**Checkpoint**: Step 3 is complete.

---

## Phase 6: User Story 4 - Change what is in the split view (Priority: P3)

**Step**: 4

**Goal**: The usual ways of opening, drops, and next and previous work on the split view.

**Independent Test**: Quickstart Q4.1–Q4.8.

### Tests for User Story 4

- [ ] T042 [P] [US4] Extend `tests/test_playlistview.cpp`: `nextRowExcluding(ids, repeat)` and `previousRowExcluding(ids)` skip the given entry ids in view order (with search, filter, sort and collapsed groups as `nextRow` and `previousRow` do), wrap only with `repeat`, and return −1 when nothing is left.

### Implementation for User Story 4

- [ ] T043 [US4] Add `nextRowExcluding` and `previousRowExcluding` to `playlistview.h` / `.cpp`. T042 passes.
- [ ] T044 [US4] Route the usual opens in a split view to the selected tile in `Main.qml` (`openAndPlay`, `openUrls` including the command line and second launch, the file dialog's `onAccepted`, `selectPlaylistItem`): the new entry replaces the selected tile's video with `tileGrid.replace`; an entry already in a tile selects that tile instead (FR-007).
- [ ] T045 [US4] Route drops in a split view in `handleDroppedUrls` / the window's `DropArea`: `tileGrid.slotAt` under the drop point; a tile → replace its video with the first dropped item; an empty cell → add tiles up to four; anything beyond four is refused with the snackbar "A split view holds up to four videos" (FR-002, FR-007).
- [ ] T046 [US4] Make next and previous in a split view replace the selected tile's video with the next or previous entry not in a tile: the play-next queue first (`playList.takeQueued`, skipping ids in a tile), then shuffle (`shuffleOrder.next` / `previous`, repeated while it lands on an entry in a tile, at most `playList.count` times) or `playlistView.nextRowExcluding` / `previousRowExcluding` (FR-024a).
- [ ] T047 [US4] Run quickstart Q4.1–Q4.8 and the camera checks QC.1–QC.5 headless against MediaMTX with `wall1`–`wall4`, `cam` with a wrong password and `rtsp://127.0.0.1:8599/x`.

**Checkpoint**: Step 4 is complete: every story works.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Documentation and full validation

- [ ] T048 [P] Update `README.md`: the split view (Play in split view from the playlist, up to four videos, click or 1–4 to choose which is heard, double-click to fill and again for full screen, Esc, Stop closes a tile, View > End split view).
- [ ] T049 [P] Update `CLAUDE.md`: `TileGrid` and the four slots with `activeMedia`; `RtspOpenGate` and the shared TCP memory (why RTSP opens are one at a time); per-tile repeat and A-B in `MediaComponent`.
- [ ] T050 Run `just format-cpp` on the new C++ files only and `just lint`; resolve every finding on lines this feature added.
- [ ] T051 Run the whole of [quickstart.md](./quickstart.md), including Q1.8 (SC-003) on a desktop with hardware decoding, and record the results in this file and the pull request description, saying plainly which of Windows, macOS and the packages were not run.

---

## Delivery order

| Step | Content | Tasks | Done when |
|---|---|---|---|
| 1 | Groundwork, no visible change | T001–T015 | All tests pass; single-view checks unchanged |
| 2 | User Stories 1 and 2 | T016–T034 | Q1.x, Q2.x |
| 3 | User Story 3 | T035–T041 | Q3.x |
| 4 | User Story 4, polish | T042–T051 | Q4.x, QC.x, full quickstart |

Each step can be its own pull request under #205, or all can go on `205-split-view`.

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: none; needed by the camera checks (T041, T047).
- **Foundational (Phase 2)**: blocks every story. Inside it: T002–T005 before their units
  (T006–T010); T011 before T012; T013 and T014 after T011; T015 last.
- **US1 (Phase 3)** and **US2 (Phase 4)**: after Phase 2. US2's selection (T028–T032) needs a
  split view to exist (T019), so in practice US1 comes first; they ship together.
- **US3 (Phase 5)**: after US2 (it builds on selection and the Esc chain).
- **US4 (Phase 6)**: after US2; independent of US3.
- **Polish (Phase 7)**: after the stories it documents.

### Within Each User Story

- Tests before the code that makes them pass (T016 → T017, T042 → T043).
- C++ before the QML that calls it.
- The quickstart task last.

### Shared Files

`Main.qml` is touched by T013, T014, T019, T021–T026, T029, T030, T032, T036–T040 and T044–T046;
these run one after another. `MediaComponent.qml` by T011, T013, T028, T031 and T035.
`MediaControlsComponent.qml` by T011, T012, T021 and T026.

### Parallel Opportunities

- T002, T003, T004 and T005 (four test files).
- T010 alongside T006–T009 (different units).
- T016 and T020 alongside T018 (model test, row, context menu); T022 alongside T021.
- T042 alongside the start of Phase 6.
- T048 and T049.

---

## Parallel Example: Phase 2

```text
# The four test files, together:
T002 tests/test_tilegrid.cpp
T003 tests/test_rtspopengate.cpp
T004 tests/test_rtspconnection.cpp
T005 tests/test_previewimageprovider.cpp

# Then, together:
T006 tilegrid.h / .cpp
T007 rtspopengate.h / .cpp
T010 previewimageprovider.h / .cpp
```

## Parallel Example: User Story 1

```text
T016 tests/test_playlistmodel.cpp
T020 PlaylistRow.qml
T022 TitleBar.qml
```

---

## Implementation Strategy

### MVP First

1. Phase 2 (groundwork) and confirm nothing changed.
2. Phases 3 and 4 together: a split view that plays, with selection and sound. This is the
   smallest version worth using, because a grid without a choice of sound is noise.
3. Stop and validate with Q1.x and Q2.x.

### Incremental Delivery

1. Step 1 → nothing visible; safe to merge on its own.
2. Step 2 → the split view (MVP).
3. Step 3 → focus and full screen.
4. Step 4 → opening, dropping, next and previous; full quickstart.

---

## Notes

- [P] tasks touch different files and do not wait on unfinished tasks
- The numbers in [research.md](./research.md) were measured headless on Linux x64 with software
  decoding; SC-003 needs a desktop run (T051)
- RTSP opens are one at a time across tiles (research R4); a camera that does not answer delays
  the ones after it by up to 10 s with Automatic
- Commit after each task or logical group, on `205-split-view`, never on `main`
