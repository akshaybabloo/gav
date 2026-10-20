# Contract: Split View

What the user can do with a split view and what GAV does in return, and what the window's QML
can rely on from `TileGrid`. Covers FR-001 to FR-032 and SC-001 to SC-007.

## Ways in and out

| Action | With nothing playing | With one video playing | In a split view |
|---|---|---|---|
| Playlist context menu, "Play in split view", on a selection (or the clicked row) | Starts a split view of those entries, if 2–4 | The playing video stays as position 1, selected, without restarting; the entries join it | The entries are added as tiles |
| The same, when the result would be 1 video or more than 4 | Unavailable | Unavailable | Unavailable |
| View menu, "End split view" | Not shown | Not shown | Keeps the selected video in the single view; the others stop |
| Stop (control bar, media keys) | As today | As today | Closes the selected tile |
| File > Open, Open URL, command line, starting GAV again, Play in the playlist | As today | As today | Replaces the selected tile's video; an entry already in a tile selects that tile |
| Drop a file onto a tile | As today | As today | Replaces that tile's video |
| Drop a file onto an empty cell (two or three videos) | — | — | Adds a tile |
| Next, previous | As today | As today | Replace the selected tile's video with the next or previous entry not in a tile (queue, then shuffle or view order) |

Entries already in a tile are not added again and do not count towards the four. With one tile
left, GAV is in the single view and everything is as before this feature.

The menu item texts:

| Where | Text |
|---|---|
| Playlist context menu | "Play in split view" |
| Its tooltip when unavailable for being more than four | "A split view holds up to four videos" |
| View menu | "End split view" |
| Message when a drop or an open would make a fifth | "A split view holds up to four videos" |

## Layout

| Videos | Cells |
|---|---|
| 1 | Whole area (single view) |
| 2 | Left half, right half |
| 3 | 2×2: top left, top right, bottom left; bottom right empty |
| 4 | 2×2 |

Positions are numbered 1 to 4 left to right, top to bottom. Each picture keeps its shape inside
its cell. The playlist panel, when open, lies over the right side of the area as it does today;
the cells do not move (FR-003a).

## Selection and sound

- Exactly one tile is selected; it alone is heard, at the window's volume and mute state.
- The selected tile has a 2-pixel border in the theme's accent colour, drawn only in a split view.
- Click a tile → it is selected. Keys 1–4 → the tile at that position is selected.
- The control bar, the keyboard shortcuts, the media keys, the window title, the seek bar, the
  statistics overlay and the operating system's now-playing display all follow the selected tile.
- Each tile keeps its own position, speed, tracks, subtitles, quality, repeat and A-B points,
  zoom and pan, brightness and contrast. The control bar shows the selected tile's.
- Every tile shows its own subtitles.
- The playlist row of an entry in a tile shows its position; the selected one is the current row.

## Focus and full screen

| State | Double-click a tile | Esc | 1–4 | F |
|---|---|---|---|---|
| Grid | That tile fills the area and is selected; the other playing files pause; live tiles carry on hidden | As today | Select | Grid full screen (as today) |
| Focused | Full screen | Back to the grid | That position fills the area instead | Toggle full screen |
| Focused, full screen | Leave full screen and back to the grid | Leave full screen, still focused | That position fills the area instead, still full screen | Leave full screen, still focused |

Back in the grid every tile is in its old place, and only the files the focus paused play on,
muted unless selected. Stop while focused closes the focused tile and brings the grid back.

Outside a split view, double-click toggles full screen as it does today.

## Each tile's life

- When a tile's video ends, fails or is lost, the tile keeps its place, showing the end or the
  failure; no tile moves and nothing moves on to the next entry.
- Each tile shows its own buffering, "Reconnecting…" and failure state, whether or not it is
  selected; the live badge and the quality menu are on the control bar and follow the selected
  tile.
- Failure messages in a split view start with the entry's title, for example
  "Camera 2: The stream was lost."
- RTSP opens happen one at a time across all tiles. Each uses its own transport choice (the
  setting and the session's memory of that source, which all tiles share) and gets its own
  failure kind. When an HTTP open overlapped a camera's open, that camera reports a
  failure with the general message, never the other source's reason.
- No resume prompt appears in a split view; positions are saved as usual.
- The mini player is unavailable while more than one tile plays.
- Nothing about the split view is saved for the next start.

## TileGrid, as QML sees it

| Member | Kind | Meaning |
|---|---|---|
| `count` | property, int | Filled slots |
| `splitView` | property, bool | `count ≥ 2` |
| `selectedSlot` | property, int | |
| `focused` | property, bool | A tile fills the area |
| `focusedSlot` | property, int | −1 when not focused |
| `entryIds` | property, list of int | Entry id per slot, −1 when empty |
| `positionOf(slot)` | function | 1–4, or 0 |
| `cellFor(slot, width, height)` | function | Rectangle; empty for a slot that is hidden |
| `slotAt(x, y, width, height)` | function | Slot under the point, or −1 |
| `canAdd(ids)` | function | Whether adding keeps four or fewer |
| `open(id)`, `start(ids)`, `add(ids)`, `replace(slot, id)`, `close(slot)`, `end()` | functions | As in [data-model.md](../data-model.md) |
| `select(slot)`, `pressPosition(p, fullScreen)` | functions | |
| `doubleClick(slot, fullScreen)`, `escape(fullScreen)` | functions | Return `{ handled, enterFullScreen, leaveFullScreen }` |
| `pauseForFocus(slot)` / `takeResumeAfterFocus()` | functions | Record and hand back the files the focus paused |
| `entryRemoved(id)` | function | Close its tile |
| `changed()` | signal | Any of the above changed |
