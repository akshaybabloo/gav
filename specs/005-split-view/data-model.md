# Data Model: Split View for up to Four Videos

Nothing here is persisted (FR-030). The playlist file formats, `history.json`, `session.m3u8` and
the settings are unchanged.

## TileGrid (new, C++, exposed to QML)

The state of the video area. One instance in the window.

| Field | Meaning | Rules |
|---|---|---|
| `slots[4]` | The playlist entry id held by each slot, or −1 | A slot keeps its player while its entry is in the grid. An entry id appears in at most one slot. Slot 0 is the single view's slot. |
| `order` | Slots in screen order, positions 1 to `count` | New entries take the next positions; closing a slot removes it from the order and the rest keep their relative order (FR-004, FR-005). |
| `count` | Number of filled slots, 0 to 4 | A split view is `count ≥ 2`. |
| `selectedSlot` | The slot that is heard and controlled | Always a filled slot when `count ≥ 1`. Becomes the first position after a close, and the focused slot on focus (FR-008, FR-017). |
| `mode` | `Grid`, `Focused` | `Focused` only in a split view. Whether the window is full screen is the window's own state, passed in where it matters. |
| `focusedSlot` | The slot filling the area in `Focused` | Equal to `selectedSlot` while focused. |
| `pausedByFocus` | Set of slots that the focus paused | Only files that were playing. Played again, and cleared, on return to the grid (FR-021). |

**Derived**

- Layout: 1 → 1×1, 2 → 2 columns × 1 row, 3 or 4 → 2×2.
- `cellFor(slot, width, height)`: the rectangle of that slot's position; the whole area for the
  focused slot in `Focused`, and an empty rectangle (hidden) for the other slots.
- `slotAt(x, y, width, height)`: the slot under a point, or −1 over an empty cell (drops).
- `playableCount(adding)`: whether adding these entries keeps `count ≤ 4` (FR-002).

**Operations and results**

| Operation | Effect |
|---|---|
| `open(entry)` | With `count = 0`, puts the entry in slot 0 (`count` 1, selected). With `count = 1`, replaces slot 0's entry. Every open in the single view goes through it. |
| `start(entries)` with `count = 0` | Fills slots 0… in playlist order; position 1 selected. Needs 2–4 entries. |
| `add(entries)` with `count ≥ 1` | Entries already in a slot are left out; the rest take the next free slots and positions; selection unchanged. Refused as a whole when the result would exceed 4 (FR-001, FR-002). |
| `replace(slot, entry)` | The slot's entry changes; position and selection unchanged. If the entry is already in another slot, that slot is selected instead and nothing is replaced (FR-007). |
| `close(slot)` | Removes the slot from the order; selection moves to position 1; `Focused` returns to `Grid`. With one slot left, that slot is the single view (FR-005). |
| `end()` | Keeps only the selected slot (FR-005, User Story 1 scenario 8). |
| `select(slot)` | Changes the selected slot (FR-010). |
| `pressPosition(p, fullScreen)` | In `Grid`: select. In `Focused`: focus that position instead. No slot at `p`: nothing (FR-010). |
| `doubleClick(slot, fullScreen)` | Returns the new mode and a full-screen change, per [research R8](./research.md). |
| `escape(fullScreen)` | In `Focused`: leave full screen if on, otherwise return to `Grid`. In `Grid`: not handled (the window's Esc chain continues). |
| `entryRemoved(id)` | Closes its slot (spec edge cases). |

**States**

```text
            double-click T                double-click T
   Grid ─────────────────────► Focused ─────────────────► Focused + full screen
    ▲                              │  ▲                         │      │
    │  Esc, close, end             │  │  Esc (leaves full screen)│      │
    └──────────────────────────────┘  └─────────────────────────┘      │
    ▲                                                                  │
    └──────────────────── double-click T (leaves full screen) ─────────┘
```

## Tile (QML, one per slot)

The existing `MediaComponent`, four of them for the whole session (research R6), with what it
gains:

| Field | Meaning |
|---|---|
| `slot`, `entryId` | Which slot and playlist entry it plays |
| `selected` | Its slot is `selectedSlot`; shows the selection mark in a split view (FR-011) |
| `repeatMode`, A-B points | Moved here from the window and the control bar (research R3) |
| `audioOutput.volume` | The window's volume |
| `audioOutput.muted` | `!selected` or the window's mute (FR-009, FR-012) |
| `isLive` | From its player; decides pause or carry on when another tile is focused |

Handlers that used `playList.currentId` (loaded, error, RTSP failure, checkpoint, subtitle error)
use `entryId` (research R2).

## RTSP open gate (new, C++)

| Field | Meaning | Rules |
|---|---|---|
| `holder` | The connection whose attempt is opening, or none | Set when an attempt is let through; cleared when Qt reports that open loaded or failed, or when the attempt is cancelled (close, replace). |
| `waiting` | Connections waiting, in arrival order | The first waiting one is let through when the gate clears. A cancelled one leaves the queue. |
| `otherOpens` | HTTP and HTTPS opens in flight | Raised when a player sets an HTTP or HTTPS source, lowered when Qt reports it loaded or failed. Local files are not counted (research R4). |
| `overlapped` | Whether `otherOpens` was above 0 at any time during the holder's open | Reset when a new holder is let through. |

An RTSP attempt sets the transport variable and the source only once it holds the gate (research
R4). A failure note taken while holding the gate belongs to the holder, unless `overlapped` is
set; then the holder reports its failure as "other".

## RTSP transport memory (changed)

The set of sources (`host:port`) that needed TCP moves from each `RtspConnection` into one store
for the process. Every connection reads and adds to it; a change of the transport setting clears
it.

## PlaylistModel (changed)

| Addition | Meaning |
|---|---|
| `TilePositionRole` (`tilePosition`) | 0 when the entry is not in the split view, else its position 1–4 |
| `setTileEntries(ids)` | The entry ids in position order; updates the role |

`currentId` and `currentRow` remain the selected tile's entry.

## PlaylistView (changed)

| Addition | Meaning |
|---|---|
| `nextRowExcluding(ids, repeat)`, `previousRowExcluding(ids)` | As `nextRow` and `previousRow`, skipping the given entry ids |

## PreviewImageProvider (changed)

Image names gain the owning player's id as a prefix; `clearImages(owner)` clears one player's
images only (research R5).
