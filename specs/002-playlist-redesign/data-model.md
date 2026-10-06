# Data Model: Playlist Redesign

**Feature**: [spec.md](./spec.md) | **Research**: [research.md](./research.md)

Nothing here is stored in a database. Entries live in memory, are written to M3U playlist files,
and a few view preferences go into the existing QML `Settings`.

## Playlist entry

One thing that can be played. Owned by `PlaylistModel`.

| Field | Type | Notes |
|---|---|---|
| `id` | integer | Assigned when the entry is added, counting up from 1. Unique for the session, never reused, not saved. Selection, queue, shuffle and undo refer to entries by id. |
| `location` | URL | A local file URL or an `http`/`https` address. Required. |
| `title` | text | From the playlist's `#EXTINF` title, otherwise the file name or the address. Never empty. |
| `kind` | `LocalVideo`, `LocalAudio`, `Stream` | Local kinds come from the file extension. Anything with an `http`/`https` location is `Stream`. |
| `streamState` | `Unknown`, `OnDemand`, `Live` | Only meaningful for `Stream`. Starts `Unknown`; set when the entry has been loaded once. |
| `group` | text | From `group-title` or `#EXTGRP`. Empty when the playlist gives none. |
| `logo` | URL | From `tvg-logo`. Kept only if `http`/`https`. Empty otherwise. |
| `durationMs` | integer | `-1` when unknown. From `#EXTINF` (seconds), or reported by the player. |
| `availability` | `Unknown`, `Playable`, `Unavailable` | `Unavailable` carries `reason`. |
| `reason` | text | Why the entry is unavailable. Empty otherwise. |
| `attributes` | text | The raw text between the duration and the title on the `#EXTINF` line, kept so unknown attributes survive a save. |

**Validation**

- `location` must be a local file URL or `http`/`https`. Anything else is rejected when added.
- A remote playlist cannot add local-file entries (unchanged from the existing loader).
- `durationMs` is `-1` or greater than zero.

**State transitions**

```text
availability:  Unknown ──load succeeds──▶ Playable
               Unknown ──load fails / file missing──▶ Unavailable(reason)
               Unavailable ──load succeeds on retry──▶ Playable
               Playable ──later load fails──▶ Unavailable(reason)

streamState:   Unknown ──loaded, has duration and can seek──▶ OnDemand
               Unknown ──loaded, no duration──▶ Live
```

## Playlist model (`PlaylistModel`)

The ordered list of entries plus the state that belongs to the playlist rather than to a view.

| State | Type | Notes |
|---|---|---|
| entries | ordered list of entries | Playlist order. This is what gets saved. |
| `currentId` | entry id or none | The entry that is loaded in the player. Survives reordering. |
| queue | ordered list of entry ids | "Play next" requests. Session only. Ids of removed entries are dropped. Played in the order queued whatever the view shows; an entry that is unavailable when its turn comes is skipped and dropped. |
| undo step | removed entries with their former rows | At most one. Replaced by the next removal, cleared by any insert, move or clear. |

**Rules**

- Removing the current entry clears `currentId` but does not stop playback. The model remembers the
  row it occupied (the anchor) until another entry becomes current, so "next" continues with the
  entry that followed it and "previous" with the one before.
- `move` keeps the moved entries in their relative order and never changes ids.
- `removeDuplicates` compares normalised locations (local paths cleaned and case-folded where the
  file system is case-insensitive; addresses compared after normalising scheme and host case) and
  keeps the first of each.
- Clearing the playlist clears the queue, the undo step and `currentId`.
- Undoing a removal restores the entries and their ids only. If the playing entry was removed and
  restored, it is listed again but not marked as playing until it is started again.
- Availability is not saved in playlist files. On load a missing local file is marked unavailable
  again; every other entry starts as `Unknown`.

## Playlist view (`PlaylistView`)

How the user is looking at the playlist. It never changes playlist order.

| State | Type | Default | Saved |
|---|---|---|---|
| `searchText` | text | empty | Session |
| `filter` | `All`, `LocalFiles`, `Streams` | `All` | Session |
| `sortOrder` | `PlaylistOrder`, `Title`, `Duration` | `PlaylistOrder` | Session |
| `grouped` | boolean | off | Session |
| collapsed groups | set of group names | empty | Session |
| selection | set of entry ids, plus an anchor id | empty | No |

**Rows**: a flat list. Each row is one of:

- **Entry row**: refers to one entry by id and source row.
- **Group header row**: `group` (name, or the catch-all for entries without one), `count` of
  entries in the group that pass the search and filter, `collapsed`.

**Derived**

- `matchCount`: entries that pass the search and filter, including those inside collapsed groups.
- `canReorder`: true only when `sortOrder` is `PlaylistOrder`, `searchText` is empty, `filter` is
  `All` and `grouped` is off.
- Visible entry order: the entry rows in view order. Entries in collapsed groups are not visible.
  This is the order "next", "previous" and shuffle use.

**Rules**

- Sorting by duration puts unknown durations last. Ties keep playlist order. Sorting by title is
  case-insensitive and puts numbers in counting order.
- Grouping takes effect only when at least one entry has a group.
- When the current entry is hidden by the search, filter or a collapsed group, "next" starts at
  the first visible entry and "previous" does nothing.
- When grouping is on, groups appear in the order their first entry appears in the playlist, with
  the catch-all last, and the sort order applies inside each group.
- Selection only ever contains ids that exist. Entries hidden by a later search stay selected
  until the selection is changed, but bulk actions apply to visible selected entries only.

## Settings

Stored in the existing QML `Settings`.

| Key | Type | Default | Meaning |
|---|---|---|---|
| `playlistPanelOpen` | boolean | `false` | Whether the panel is open while a video is showing. |
| `playlistPanelWidth` | integer | `360` | Panel width in pixels. Clamped to 280 … 60 % of the window on use. |
| `showChannelLogos` | boolean | `false` | Whether logo images may be downloaded (FR-012). |

## Logo cache

| Item | Value |
|---|---|
| Location | `<CacheLocation>/logos/` |
| File name | SHA-256 of the logo address, `.png` |
| Content | The decoded image scaled to the row icon size. The address is not stored. |
| Limit | 20 MB, oldest files removed first |
| Cleared by | The existing "clear" control under Settings → History, and by deleting the cache directory |

## Playlist file

See [contracts/playlist-format.md](./contracts/playlist-format.md). The session playlist uses the
same format and gains the same fields.

## Relationships

```text
PlaylistModel 1 ── * PlaylistEntry
PlaylistModel 1 ── 0..1 current entry (by id)
PlaylistModel 1 ── * queued entry ids
PlaylistView  1 ── 1 PlaylistModel (source)
PlaylistView  1 ── * rows (entry row → PlaylistEntry, header row → group name)
ShuffleOrder  ── visible entry ids from PlaylistView
LogoProvider  ── PlaylistEntry.logo (only when showChannelLogos is on)
```
