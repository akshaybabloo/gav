# Research: Split View for up to Four Videos

Measurements were taken on 2026-10-21 on Linux x64 (Intel Core Ultra 7 155H, 22 threads, Intel
and NVIDIA graphics) with Qt 6.12.0 and its stock FFmpeg plugin, in silent headless runs
(offscreen platform, no audio device). Source references are to Qt 6.12.0's `qtmultimedia`.

## R1. Can one GAV process play four videos at once? (FR-001, SC-003)

**Measured**: a temporary hook created four extra `CustomMediaPlayer`s, each with its own
`VideoOutput` and muted `AudioOutput`, in a 1920×1200 window, playing four different 90-second
1080p 30 fps H.264 files (2×2 cells of 960×540), with statistics on.

| Players | Frame rate after 30 s and 60 s | Dropped frames | Processor (one core = 100%) | Memory (RSS) |
|---|---|---|---|---|
| 1 | 30, 30 | 0 | 17–18% | 408 MB |
| 4 | 30, 30, 30, 30 (each) | 0 in each | 22–30% | 926 MB |

The offscreen platform has no graphics device, so every player reported "Software" decoding and
rendering stayed on the processor; with a display, Qt uses hardware decoding where it can. The
test pattern is cheap to decode compared with camera footage.

**Decision**: one `CustomMediaPlayer` per tile, all in the main process, as the single video is
today. Each extra player costs about 170 MB here.

**Rationale**: four players run side by side with no frame loss and modest load. Nothing in
Qt's backend is shared between players except what R4 and R5 list.

**Alternatives considered**: one decoder process per tile (heavier, and playback has never been
isolated in GAV; Constitution II asks for isolation of batch and probing work, which tiles do not
add); compositing four streams into one picture (not possible with Qt's player, and it would
lose per-tile tracks and subtitles).

**Still to check by hand** (quickstart): SC-003 on a desktop with hardware decoding and a real
display, and with camera footage.

## R2. What in the window assumes one player? (FR-013, FR-015, FR-025, FR-031)

`Main.qml` refers to `mediaComponent` 116 times. By use:

| Use | Examples | In a split view |
|---|---|---|
| Controls and dialogs acting on "the" player | control bar, track and quality menus, go to time, frame step, chapters, statistics overlay, screenshot, volume and mute, media keys (`MediaSession`), window title | The selected tile |
| Reactions to one player's events | `onMediaLoadedChanged` (record recent file, resume prompt, autoplay, pending subtitle files, `setLoaded`), `onErrorOccurred` and `onRtspFailed` (mark `playList.currentId` unavailable, show a message), `onPositionCheckpoint`, subtitle errors, `rtspTransport` binding | Every tile, each for its own entry |
| Playback driven by the playlist's current entry | `playList.currentRow` change → `mediaComponent.path`; `selectPlaylistItem`, `nextItem`, `previousItem` | Decided by the split view (R9) |
| Whole-window things | `onAboutToQuit` checkpoint, mini player | Every tile; mini player off in a split view |

**Decision**: the window keeps one name for "the player the controls act on",
`activeMedia`, bound to the selected tile's `MediaComponent`. The 116 references become
`activeMedia` where they act on the selected video. The handlers in the second row move into the
tile itself and use the tile's own entry id instead of `playList.currentId`. With one tile,
`activeMedia` is that tile and every behaviour is as today (FR-031).

**Alternatives considered**: keeping `mediaComponent` and adding a separate grid beside it (two
code paths for playing a video, which FR-031 would then have to prove equal twice); a C++
"player proxy" forwarding every property (large, and QML bindings already re-evaluate when
`activeMedia` changes).

## R3. Per-video state that lives outside the player today (FR-014, FR-024a)

| State | Where it is today | Problem with several videos |
|---|---|---|
| Repeat mode (none, once, loop, A-B) | `mainWindow.repeatMode`, and the end-of-media handling in `MediaControlsComponent` (`Connections` on the bound player) | Only the bound (selected) player would repeat; the others would ignore their repeat |
| A-B points | the seek bar's range slider | One pair for the window |
| Moving on at the end | the same handler calls `nextTrack()` | Must not happen in a split view (FR-024a) |
| Brightness, contrast, zoom and pan | inside `MediaComponent` (`VideoOutput` properties) | Already per tile |
| Subtitles, tracks, quality, chapters | inside `CustomMediaPlayer` (its `SubtitleController`, `StreamQuality`) | Already per tile |
| Speed | the player; new videos start at `appSettings.playbackRate` | Already per tile; the saved default stays global |
| Volume and mute | the one `AudioOutput` in `MediaComponent`, mirrored to settings | Must be one level for the window (FR-012) |

**Decision**: repeat mode, A-B points and the end-of-media handling move into `MediaComponent`,
so each tile repeats on its own; the control bar shows and changes the selected tile's values.
Moving on at the end happens only with one tile. Each tile's `AudioOutput` takes the window's
volume and is muted unless it is the selected tile (R7).

## R4. RTSP state shared by the whole process (FR-027, SC-005)

Feature 004 was built for one player. Three parts do not hold with several:

1. **The transport.** GAV sets `QT_FFMPEG_RTSP_TRANSPORT` and then the source. Qt reads the
   variable later, on a worker thread, when it builds the open options
   (`qffmpegmediadataholder.cpp`, `loadMedia`, run from `QtConcurrent::run` in
   `QFFmpegMediaPlayer::setMedia`). A second player setting the variable in between makes the
   first open with the wrong transport.
2. **The failure reason.** The reason (sign-in refused, not found) is known only from Qt's
   warning "Could not open media. FFmpeg error description: …", which names no source and is
   logged on the worker thread. `RtspConnection::noteOpenFailure` keeps one note for the process.
   Two opens failing together can swap their reasons.
3. **The TCP memory.** "A source that needed TCP is opened with TCP for the rest of the session"
   is a `QSet` inside each `RtspConnection`, so each tile would learn it separately.

**Decision**:

- An RTSP open gate shared by all players: an RTSP attempt waits while another player's RTSP
  attempt is opening, and an attempt holds the gate from setting the variable until Qt reports
  the source loaded or failed. This makes both the variable and the failure note belong to one
  open at a time. Files and HTTP sources do not use the gate.
- Other sources write to the same failure note, because Qt's warning is the same for every
  source. The gate therefore also counts non-RTSP opens in flight. If one of them overlapped the
  holder's open, the holder treats a noted failure as "other" instead of sign-in or not found. A
  camera can then show the general message instead of the specific one, but never another
  source's reason.
- The TCP memory becomes one store for the process, read and written by every
  `RtspConnection`; changing the setting clears it (as `forgetTransports` does now).
- Both are plain C++ with no playback code and are unit-tested.

**Cost**: opens are one after another. An open to a reachable source took about 1.9 s from the
open to the first picture on 2026-10-14 against MediaMTX, so four cameras on a local network
are all playing after about 8 s. A camera that does not answer holds the gate for its network
timeout (5 s per attempt, 10 s with Automatic), delaying the cameras after it.

**Alternatives considered**:

- FFmpeg's legacy `?tcp` / `?udp` address suffix instead of the variable: removes the first
  problem only; the failure note still needs one open at a time. Not measured.
- Telling failures apart by order of arrival: the warning and the player's error are delivered
  on different threads, so the order is not guaranteed.
- A patch to Qt's plugin carrying the reason with the error: does not reach development builds,
  which use the stock plugin (same reason as in feature 004's research, R14).

## R5. Hover previews are shared (FR-031)

`PreviewImageProvider` is one object for the process. Image names are
`preview_<milliseconds>`, and `resetPreviewPlayer` (called on every stop) clears every stored
image.

**Decision**: each player prefixes its image names with its own id and clears only its own
images. Closing one tile then no longer removes the selected tile's seek-bar preview.

## R6. Layout and tiles that keep playing while the grid changes (FR-003, FR-004, FR-005)

A tile's player must not restart when the layout changes (two to three videos, a close, a
focus). The grid therefore has four fixed **slots**, each holding one `MediaComponent` in a
`Loader` that is active only while the slot has a video. A slot keeps its player for as long as
its video is in the split view; only the slot's **position** on screen changes. Slot 0 is the
single view's player, so a single video uses one player exactly as today.

Cells: one video → the whole area; two → left and right halves; three or four → 2×2, filled left
to right, top to bottom, the fourth cell empty with three. Each `VideoOutput` already keeps the
picture's shape inside its item.

**Decision**: a C++ unit `TileGrid` holds the slots, their order, the selected and focused slot
and the rules for adding, replacing, closing, selecting and focusing, and computes each slot's
cell for a given area size. QML places the four `Loader`s from it.

**Alternatives considered**: a `Repeater` over a model of videos (closing a tile would shift the
model and recreate the players after it); building tiles with `createObject` (lifetime handled
by hand, harder to test).

## R7. Moving the sound (FR-009, FR-012, SC-002)

**Decision**: every tile has its own `AudioOutput` at the window's volume; all are muted except
the selected tile's, and that one follows the window's mute state. Changing the selection only
flips two `muted` values, which takes effect at once.

**Alternatives considered**: giving the audio output only to the selected tile's player and
none to the others (saves decoding sound that is not heard, but Qt rebuilds the audio path when
the output changes, which takes noticeably longer and can glitch).

## R8. Focus, full screen and Esc (FR-017 to FR-023, FR-010)

States of the video area in a split view: **grid**, **focused** (one tile fills the area) and
**focused full screen**. Full screen is the window's visibility, which other actions (F, the
title bar) also change.

| In state | Double-click on tile T | Esc | 1–4 (position P) | F (full screen) |
|---|---|---|---|---|
| Grid | Focus T, select T | As today (search, then playlist panel, then leave full screen) | Select P | Toggle the grid full screen |
| Focused | Full screen | Back to grid | Focus P instead | Toggle full screen |
| Focused full screen | Leave full screen, back to grid | Leave full screen, stay focused | Focus P instead | Leave full screen, stay focused |

On focus, every other tile that is a file and is playing is paused and remembered; live tiles
carry on, hidden. On the way back to the grid, only the tiles that the focus paused are played
again. A tile the user had paused stays paused.

**Decision**: `TileGrid` answers "what does this gesture do" from the state and whether the
window is full screen, and returns the new state plus whether to enter or leave full screen;
QML applies it. The Esc shortcut asks the split view before its last step (leaving full screen).
Outside a split view, double-click keeps calling `toggleFullScreen()`.

## R9. The playlist and the split view (FR-001, FR-015, FR-024a, FR-007)

Today the playlist's current entry drives playback: changing `playList.currentRow` loads that
entry into the one player. In a split view the current entry is the **selected tile's** entry,
so selecting a tile must change the current entry without loading anything.

**Decisions**:

- The model gets a role giving each entry's position in the split view (0 = not in it, 1–4), for
  the row to show; the current entry stays the selected one.
- The handler that loads the current entry checks the split view first: an entry already in a
  tile is selected, not loaded again (FR-007).
- Next and previous in a split view replace the selected tile's video with the next or previous
  entry not already in a tile, in the order they use today: the play-next queue first, then
  shuffle order or the view's order. `PlaylistView` gets a "next row skipping these entries"
  variant, and shuffle's choice is skipped forward while it lands on a tile's entry. Removing
  entries from the playlist closes their tiles.
- "Play in split view" is a playlist context-menu item acting on the selection (or the clicked
  row), unavailable when the result would be one video or more than four.

## R10. Opening and dropping during a split view (FR-007)

The usual ways to open (`openAndPlay`, `openUrls`, the file dialog, the command line and second
launch through `openUrls`, Play in the playlist) end in "make this the current entry". In a split
view they replace the selected tile's video. The window's drop area knows the drop point, so a
drop asks `TileGrid` which slot is under it: a tile → replace that tile's video; empty space
(only possible with two or three videos) → add a tile.

## R11. What stays a single-view feature (FR-028, FR-029, FR-030)

- **Resume prompt**: `promptResumeIfSaved` runs only with one tile. Positions are still saved
  by every tile through its own checkpoint handler.
- **Mini player**: it moves the one player's video output into another window; disabled while
  more than one tile plays.
- **Session**: `session.m3u8` keeps the playlist only; no split-view state is written.

## R12. Messages that name their video (FR-025)

`playbackErrorDialog` says "Unable to play this file." and the stream snackbar shows the
address. In a split view both add the entry's title; the tile that failed keeps its place and
shows its failure (FR-024). The playlist marks the right entry unavailable because the handler
now uses the tile's entry id (R2).

## R13. How this is tested (Constitution V)

| What | How |
|---|---|
| `TileGrid`: add, replace, close, order, selection, focus states, gestures, keys, cells for each count and size, pause-and-resume bookkeeping | Unit tests (`tests/test_tilegrid.cpp`) |
| RTSP open gate: one open at a time, release on loaded or failed, cancelled waiting opens, order | Unit tests |
| Shared TCP memory across two `RtspConnection`s, cleared by a setting change | Unit tests (`tests/test_rtspconnection.cpp`) |
| Playlist position role; next/previous skipping entries | Unit tests (`test_playlistmodel.cpp`, `test_playlistview.cpp`) |
| Preview image names per player, clearing only one player's | Unit test |
| Everything on screen: layout, sound, selection mark, focus, full screen, drops, messages, four cameras | [quickstart.md](./quickstart.md), with silent headless runs and screenshots where possible |

## R14. Delivery

The work splits into four steps; each leaves GAV usable:

1. **Groundwork, no visible change**: `TileGrid`, the RTSP gate and shared TCP memory, per-player
   preview names, per-tile repeat and A-B, `activeMedia` with one tile. Single-view checks of
   earlier features pass unchanged.
2. **User Stories 1 and 2**: Play in split view, layout, selection and sound, controls on the
   selected tile, Stop closes a tile, ending the split view, playlist marks, messages naming the
   video, mini player and resume prompt off in a split view.
3. **User Story 3**: focus, full screen, Esc and 1–4 while focused.
4. **User Story 4**: usual opens replace the selected tile, drops, next and previous.

They can be pull requests of their own or one branch, as the user prefers.

## R15. Not doing

- More than four videos, hand-made layouts, tiles of different sizes, synchronised playback,
  mixing sound from several tiles, a playlist per tile (spec Assumptions).
- Restoring a split view after a restart (FR-030).
- Sending RTSP `TEARDOWN` on close: a separate known limit (feature 004, R14); with four cameras
  over UDP it applies to each.
