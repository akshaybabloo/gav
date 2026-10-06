# Contract: Playlist Models (QML-facing)

The interface the QML layer uses. Both types are registered in the `gavqml` module. Names are the
contract; signatures may gain defaulted parameters during implementation. Field meanings are in
[the data model](../data-model.md).

## `PlaylistModel`

One instance, created in `Main.qml`. A list model in playlist order.

**Roles** (per row): `entryId`, `location`, `title`, `kind`, `streamState`, `group`, `logo`,
`durationMs`, `available`, `reason`, `isCurrent`, `queuePosition` (0 when not queued).

**Properties**

| Property | Type | Notes |
|---|---|---|
| `count` | int | Number of entries. |
| `currentRow` | int | Row of the current entry, `-1` if none. Writable. Notifies with `currentRowChanged`, also when the row number shifts because entries were added or removed above it. |
| `currentId` | int | Same entry by id, `-1` if none. Notifies with `currentChanged`, only when a different entry becomes current. |
| `audioExtensions` | list of strings | Extensions treated as local audio. Everything else local is video. |
| `queueLength` | int | Entries waiting in "play next". |
| `canUndoRemove` | bool | Whether a removal can be undone. |

**Methods**

| Method | Effect |
|---|---|
| `append(entries)` | Adds entries at the end in one batch. Returns the first new row, or `-1`. |
| `insert(row, entries)` | Adds entries before `row`. |
| `remove(ids)` | Removes those entries as one undo step. Returns how many were removed. |
| `undoRemove()` | Puts the last removed entries back at their former rows. |
| `move(ids, destinationRow)` | Moves those entries, as a block and in their current relative order, to before `destinationRow`. |
| `clear()` | Removes everything. Not undoable (the existing confirmation dialog stays). |
| `removeDuplicates()` | Removes later entries with a location already seen. Returns the count. Undoable. |
| `playNext(id)` | Adds the entry to the end of the play-next queue. |
| `takeQueued()` | Removes and returns the row of the first queued entry that is not unavailable, dropping unavailable ones on the way, or `-1`. The view's search and filter play no part. |
| `setLoaded(id, durationMs, isLive)` | Records what the player learned and marks the entry playable. |
| `setUnavailable(id, reason)` | Marks the entry unavailable. |
| `entryAt(row)` | The entry as a map, for code that needs one entry. Maps use `path` for the location, as `PlaylistFiles` does. |
| `rowForId(id)` / `idAt(row)` | Mapping between ids and rows. `-1` when there is none. |
| `toVariantList()` | Every entry in playlist order, for saving. |
| `locations()` | Every location in playlist order, for collage. |

**Signals**: the standard list-model signals, plus `countChanged()`, `currentChanged()`,
`currentRowChanged()`, `queueChanged()` and `undoChanged()`.

**Guarantees**

- Ids are stable across `move`, sort, filter and undo.
- `append` and `insert` of N entries emit one insert notification, not N.
- Adding or removing entries never changes which entry is current, except that removing the
  current entry leaves no current entry.

## `PlaylistView`

One instance, with `source` set to the `PlaylistModel`. The `ListView`'s model.

**Roles** (per row): `isHeader`; for header rows `group`, `groupCount`, `collapsed`; for entry
rows every `PlaylistModel` role plus `selected` and `sourceRow`.

**Properties**

| Property | Type | Notes |
|---|---|---|
| `source` | `PlaylistModel` | Required. |
| `searchText` | string | |
| `filter` | enum `All`, `LocalFiles`, `Streams` | |
| `sortOrder` | enum `PlaylistOrder`, `Title`, `Duration` | |
| `grouped` | bool | |
| `matchCount` | int | Entries passing search and filter. |
| `hasGroups` | bool | Whether any entry has a group, so the UI can hide the grouping control. |
| `canReorder` | bool | See the data model. |
| `selectionCount` | int | Visible selected entries. |
| `currentViewRow` | int | View row of the current entry, `-1` if hidden. |
| `canGoNext` / `canGoPrevious` | bool | Whether `nextRow(false)` / `previousRow()` would return an entry. For bindings. |

**Methods**

| Method | Effect |
|---|---|
| `toggleGroup(group)` | Collapses or expands a group. |
| `select(viewRow, modifiers)` | Applies click, Ctrl+click or Shift+click selection rules. |
| `selectAll()` / `clearSelection()` | Over visible entries. |
| `selectedIds()` | Ids of visible selected entries, in view order. |
| `nextRow(repeat)` | Source row to play next in view order, skipping unavailable entries, or `-1`. Wraps when `repeat` is true. With no current entry it starts from the model's anchor, or from the top. |
| `previousRow()` | The same, backwards. |
| `visibleIds()` | Visible entry ids in view order. |
| `playableIds()` | The same without unavailable entries, for shuffle. |
| `viewRowFor(sourceRow)` / `sourceRowFor(viewRow)` | Mapping. `-1` when there is none. |
| `clearSearchAndFilter()` | Resets `searchText` and `filter`. |

**Signals**: the standard list-model signals, plus `visibleEntriesChanged()` (the set or order of
visible entries changed), `playableEntriesChanged()` (an entry became available or unavailable)
and `selectionChanged()`.

**Guarantees**

- No method of `PlaylistView` changes playlist order or removes entries.
- A change of search text is applied before the call returns, or, if the rebuild is deferred
  (research R3), within 200 ms for 10,000 entries.
- `nextRow` and `previousRow` never return a header row, a row in a collapsed group, or an
  unavailable entry.

## `ShuffleOrder` (changed)

`reset`, `next`, `previous` and `setCurrent` take and return entry ids instead of row indices.
`itemInserted`, `itemRemoved` and `itemMoved` are replaced by `setCandidates(ids)`, called with
`PlaylistView.playableIds()` whenever `visibleEntriesChanged` or `playableEntriesChanged` fires, so
shuffle never picks an unavailable entry (FR-009). History entries whose ids are
no longer candidates are dropped.

## `LogoProvider`

Image provider id `logo`. Request form: `image://logo/<percent-encoded http(s) address>`.

- Returns the cached image when present, otherwise downloads it within the limits in
  [research R6](../research.md) and caches the scaled result.
- Any failure (scheme, timeout, size, decode) yields an empty image. No error is shown.
- `LogoProvider.clearCache()` empties `<CacheLocation>/logos/` and is called by the existing
  "Clear history" action.
- `LogoProvider.cancelPending()` aborts queued and in-flight downloads. QML calls it when
  `showChannelLogos` is turned off (FR-012b).
- QML must not build a `logo` request unless `showChannelLogos` is on.
