# Research: Playlist Redesign

**Feature**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md) | **Date**: 2026-10-05

Each section records a decision, why it was made and what was rejected. Findings marked
"measured" or "read" come from the current code on `main`; everything else is design intent to be
confirmed during implementation.

## R1. Where the playlist data lives

**Findings** (read)

- The playlist is a QML `ListModel` (`playList` in `Main.qml`) holding `name`, `path`, `type` and
  `icon` per row. `Main.qml` touches it 28 times and `PlayListComponent.qml` 15 times
  (`append` ×5, `clear` ×2, `get` ×5, `remove` ×1, `count` ×27).
- The current item is the `ListView`'s `currentIndex`, read 22 times in `Main.qml`.
- Search hides rows by setting the delegate height to 0, so a 10,000-entry list still creates and
  binds every delegate that scrolls past, and each keystroke re-evaluates a binding per row.

**Decision**: Move the playlist into C++ as `PlaylistModel`, a `QAbstractListModel` registered in
the `gavqml` module, with the current entry held by the model (`currentRow`) instead of by a view.
QML keeps no copy of the entries.

**Rationale**: Constitution Principle V requires playlist manipulation to be unit-tested, which a
QML `ListModel` driven from JavaScript cannot be. A C++ model also makes search, sort and grouping
cheap (R2, R3) and gives one owner for the current entry, the queue and undo.

**Alternatives considered**

- Keep the `ListModel` and add roles: no tests, and per-row JavaScript for 10,000 entries.
- QML `SortFilterProxyModel` over the `ListModel`: still untestable from C++ and cannot produce
  group header rows.

## R2. How search, filter, sort and grouping are presented

**Decision**: A second C++ model, `PlaylistView`, sits between `PlaylistModel` and the `ListView`.
It exposes a flat list of rows, each either an entry row (mapped to a source row) or a group
header row (name, count, collapsed). It owns the search text, filter, sort order, grouping flag
and the set of collapsed groups, and rebuilds its row list when any of them or the source changes.

**Rationale**

- `ListView` sections can draw group headings but cannot collapse a group or show a count without
  walking the model. Header rows in the model give both.
- One model that owns "what the user sees" is also what next, previous and shuffle need (R7).
- A full rebuild is a filter plus a stable sort over at most tens of thousands of short strings,
  which is well inside the 200 ms budget (R3).

**Details**

- Search is case-insensitive substring match on title and group, with the text pre-folded once per
  entry and cached.
- Filter values: all, local files, streams. There is no live filter: playlists do not say which
  streams are live and GAV only learns it by opening one (R9), so such a filter would list every
  unopened stream. A row is labelled "Live" only after that is confirmed.
- Sort orders: playlist order, title (locale-aware, case-insensitive), duration (unknown last).
  Sorting applies inside each group when grouping is on. Groups are ordered by first appearance
  in the playlist, with the catch-all "Ungrouped" last.
- Collapsing or expanding a group removes or inserts exactly that group's rows, and removing
  entries while the view is searched, filtered, sorted or grouped removes exactly their rows (and
  the header of a group that becomes empty), so the `ListView` keeps its scroll position.
- Changes to the search text, filter, sort order or grouping use a model reset, after which the
  list returns to the top. `layoutChanged` was planned for these and dropped during
  implementation: filtering and grouping change the number of rows, which a layout change cannot
  express to a QML view, and for a pure sort Qt's QML delegate model turns a layout change into one
  move per row. That per-row cost was read from Qt's source, not measured.
- Title sort is case-insensitive, follows the locale, and puts numbers in counting order
  ("Channel 2" before "Channel 10"). Under the `C` locale Qt's collator compares raw bytes, which
  is neither, so English rules are used there instead. CI runs the tests under the `C` locale.
- Grouping only takes effect when at least one entry has a group. Without groups the list stays
  flat and reordering stays allowed.
- A duration learned while the list is sorted by duration does not move the row until the view is
  next rebuilt, so a row does not jump away when it starts playing.

**Alternatives considered**

- `QSortFilterProxyModel` plus `ListView.section`: no collapse, no header counts, and sorting by
  group then by the chosen order needs a custom `lessThan` anyway.
- A tree model with `TreeView`: heavier delegate machinery for a two-level list and no gain.

## R3. Staying responsive with 10,000 entries (FR-028, SC-003, SC-004)

**Decision**

- Entries are appended to `PlaylistModel` in one `beginInsertRows` batch per loaded playlist.
- The `ListView` uses fixed-height rows, `reuseItems: true` and a small `cacheBuffer`, so only the
  visible rows exist as items.
- Delegates bind only to their own roles. Nothing in a delegate depends on the search text.
- `PlaylistView` rebuilds synchronously on the UI thread. If measurement shows a rebuild above
  50 ms at 10,000 entries, the rebuild moves to a worker and is applied when ready.
- Remote playlist parsing already runs off the UI thread (`PlaylistFiles`), and stays there.

**Rationale**: The cost today is per-row QML work, not the size of the data. Removing that is
enough; threading the view is kept as a measured fallback rather than built up front.

**Measured** on 2026-10-06 with `PlaylistViewTest.RebuildTimeIsLogged` in a release build (Linux
x64, one run):

| Entries | Append | Search | Clear search | Sort by title | Sort by duration | Group | Collapse | Filter |
|---|---|---|---|---|---|---|---|---|
| 10,000 | 11.8 ms | 0.1 ms | 0.1 ms | 3.4 ms | 0.5 ms | 0.9 ms | 0.7 ms | 0.7 ms |
| 50,000 | 64.3 ms | 0.7 ms | 0.5 ms | 19.1 ms | 5.4 ms | 6.6 ms | 5.3 ms | 5.9 ms |

Every rebuild is far below the 50 ms threshold at 10,000 entries, so the view stays synchronous on
the UI thread. In a debug build the worst rebuild at 10,000 entries was 9.5 ms.

## R4. The panel over the video (FR-001 to FR-004)

**Decision**

- When a video is showing, the playlist is a panel anchored to the right edge of the content
  area, drawn above the video with an opaque themed background. Default width 360 px, minimum
  280 px, maximum 60 % of the window width, resizable by dragging its left edge.
- When nothing is playing, or only audio is playing, the playlist fills the content area as it
  does today. The same component is used in both places; only its geometry changes.
- A transparent click-catcher covers the rest of the video while the panel is open. A click on it
  closes the panel and is not passed to the video, so it does not also toggle play/pause.
- Escape closes the panel first. A second Escape leaves full screen, as today.
- In full screen the panel behaves the same. The control bar's auto-hide is suspended while the
  panel is open so the two do not fight.
- If the window is narrower than twice the minimum width, the panel takes the full width and the
  click-catcher is replaced by a close button in the panel header.
- Open/closed state and width are stored in the existing QML `Settings`.
- Toggle: the existing playlist button in the control bar, plus `Ctrl+L`. `Ctrl+F` opens the panel
  if needed and focuses the search field.

**Rationale**: The user chose an overlay panel over a side-by-side split. An overlay needs no
change to how the video is laid out or zoomed, which keeps `MediaComponent.qml` untouched.

**Alternatives considered**

- Qt Quick Controls `Drawer`: it is modal-first, dims the content, and positions itself against
  the window rather than the content area, which breaks under the custom title bar.
- A translucent panel: text over moving video fails the contrast requirement (FR-013).

## R5. Group and image information in playlists (FR-006, FR-012, FR-027, SC-008)

**Findings** (read)

- `PlaylistIO::parseLines` keeps the title and whole-second duration from `#EXTINF` and discards
  everything between the duration and the title.
- IPTV playlists put attributes there: `group-title="News"`, `tvg-logo="https://…"`,
  `tvg-id="…"`, and others. Some use a separate `#EXTGRP:News` line instead of `group-title`.
- `PlaylistIO::serialise` writes `#EXTINF:<seconds>,<title>` only, so opening and saving an IPTV
  playlist today loses every attribute.

**Decision**

- `PlaylistEntry` gains `group`, `logo` and `attributes`. `attributes` is the raw text between the
  duration and the title, kept verbatim.
- Reading: `group` comes from `group-title`, or from a preceding `#EXTGRP:` line when
  `group-title` is absent. `logo` comes from `tvg-logo` and is kept only when it is an `http` or
  `https` address.
- Writing: `#EXTINF:<seconds> <attributes>,<title>`, with `group-title` inserted or updated in the
  attributes when the entry has a group. Unknown attributes are written back unchanged.
- Entries whose local file is missing are kept and marked unavailable instead of being dropped at
  load, so saving the playlist again does not lose them. The "skipped N" message becomes
  "N unavailable".

**Rationale**: SC-008 requires that opening and saving an existing playlist loses nothing.
Keeping the raw attribute text is the only way to round-trip attributes GAV does not understand.

**Alternatives considered**: Parsing every known IPTV attribute into fields: more code, still
loses the ones not listed.

## R6. Channel logos (FR-012, FR-012a, FR-012b, FR-029)

**Decision**

- A `Settings` switch, "Show channel logos", on by default (changed from off on 2026-10-06, with
  constitution v1.3.0). With it off, delegates never set an
  image source.
- With it on, delegates request `image://logo/<percent-encoded address>` only while they are
  instantiated, which with `reuseItems` and a small `cacheBuffer` means visible rows plus a few
  either side.
- `LogoProvider` is a `QQuickAsyncImageProvider` with its own `QNetworkAccessManager`:
  - `http`/`https` only, redirects limited to those schemes
  - 10 s transfer timeout, 512 KB limit per image, at most 4 requests in flight
  - decoding on a worker thread through `QImageReader` restricted to PNG, JPEG and WebP, with a
    1024 × 1024 pixel limit and an allocation limit, then scaled to the row's icon size (stored
    at up to 96 × 96 so it stays sharp on high-density screens). Qt's allocation limit is
    process-wide, so it is left at its default and only set if none is in force
  - failures return an empty image and the delegate shows the kind icon
- Cache: `<CacheLocation>/logos/<sha256 of address>.png`, holding the scaled image only, capped at
  20 MB with oldest-first eviction. The address is not stored. "Clear" in Settings → History also
  empties this cache, and turning the switch off cancels requests in flight.

**Rationale**

- A custom provider is the only way to enforce a scheme allow-list, size and time limits and a
  cache GAV controls. A plain QML `Image` with a network source uses the engine's network stack
  with none of those.
- Storing only the scaled image under a hash keeps the least identifying data (Constitution
  Principle I).

**Constitution note**: see the Constitution Check in [plan.md](./plan.md). Logos are not "needed
to play" a stream, and image decoding happens in-process. Both are recorded there.

**Alternatives considered**

- `QNetworkDiskCache`: stores the original bytes and the address in each cache file.
- Decoding in a subprocess: one helper process per batch of small images for an opt-in cosmetic
  feature is disproportionate; the allow-listed decoders are the ones Qt already runs in-process
  for cover art.

## R7. What "next" means (FR-008, FR-009, FR-018, FR-022)

**Decision**: All automatic movement goes through `PlaylistView`:

1. If the play queue is not empty, take its first entry that is not unavailable, whether or not
   the view currently shows it (FR-022a).
2. Otherwise, with shuffle on, ask `ShuffleOrder` for the next entry among the visible entries.
3. Otherwise take the next entry row after the current one in view order, skipping header rows,
   collapsed groups and unavailable entries.

"Previous" mirrors step 3, or uses the shuffle history. Repeat-all wraps within the visible
entries. If the current entry is hidden by the search or filter, "next" starts from the first
visible entry.

- `ShuffleOrder` changes from row indices to stable entry ids, and is reset with the visible id
  list whenever the set of visible entries changes, keeping its history where the ids still
  exist. Its existing tests are adapted.
- The play queue is a list of entry ids in `PlaylistModel`. It is not saved.
- The media-session "can go next / previous" flags come from the same functions.

**Rationale**: FR-018 says automatic playback follows what the user sees. One place that answers
"what is next" keeps the control bar, keyboard, end-of-media and OS media keys consistent.

## R8. Selection, reordering, undo and duplicates (FR-020 to FR-024)

**Decision**

- Selection is a set of entry ids held by `PlaylistView`, exposed as a `selected` role. It follows
  the usual rules: click selects one, Ctrl+click toggles, Shift+click extends from the anchor,
  Ctrl+A selects all visible entries.
- `PlaylistModel::remove(ids)` records the removed entries and their rows as a single undo step.
  `undoRemove()` restores them. Only the most recent removal can be undone, and the snackbar's
  existing action button offers it.
- `PlaylistModel::move(ids, destinationRow)` moves the selected entries as a block, preserving
  their relative order. Dragging is enabled only when `PlaylistView::canReorder` is true (playlist
  order, no search, no filter, grouping off). Otherwise the drag handle is disabled with a tooltip
  that says why.
- Drops from outside insert at the row under the pointer through `PlaylistModel::insert`.
- `removeDuplicates()` keeps the first entry for each normalised location and reports the count.
  It is undoable like any removal.

**Rationale**: Ids rather than rows keep selection, queue and undo correct while the view is
filtered or re-sorted.

## R9. Availability, duration and live state (FR-007, FR-009, FR-010)

**Decision**

- An entry's kind is local video or local audio (by extension, as today) or stream. For streams
  the model also holds `streamState`: unknown, on-demand or live.
- When the player finishes loading the current entry, `Main.qml` reports its duration and
  `isLive` to the model, which updates the row. Duration is saved in `#EXTINF` on the next save.
- When the player reports an error for the current entry, the entry is marked unavailable with
  the error text. A local file that does not exist is marked unavailable when the playlist is
  loaded, which happens off the UI thread. A file that disappears later is caught the same way as
  any other failure, by the player's error. There is no file-system check on the UI thread before
  playing (Constitution Principle II).
- Availability is not written to playlist files, so after a restart only missing local files are
  marked again.
- A successful load clears the unavailable mark. "Play" on an unavailable entry tries again.

**Rationale**: The spec rules out opening entries in advance to measure them, so the model learns
by use and remembers what it learned through the playlist file.

## R10. Keyboard and accessibility (FR-013, FR-025, SC-007)

**Decision**: The key map is in [contracts/playlist-panel.md](./contracts/playlist-panel.md).
Playlist keys act only while the playlist has focus, so they do not collide with the global
single-letter shortcuts. Rows expose an accessible name made of title, kind and state. The kind is
shown by icon and by text in the row's last line, never by colour alone. Text and icons meet a
contrast ratio of 4.5:1 (3:1 for large text and icons) in both themes, checked with a contrast
tool during validation, and every control has a visible focus indicator.

## R11. Replacing the old model without a flag day

**Decision**: Three steps, each leaving the app working:

1. Introduce `PlaylistModel` with the same four fields the `ListModel` has, switch `Main.qml` and
   `PlayListComponent.qml` to it, and move "current entry" into the model. No visible change.
2. Add `PlaylistView`, the new rows and the panel.
3. Add the playlist-file attributes, logos, selection and reordering.

**Rationale**: Step 1 is a pure refactor that the existing tests and quickstart for
`specs/001-player-feature-roadmap` can verify before anything new is added.

## R12. "Show in file manager" on three platforms (FR-022)

**Decision**: One function, `PlaybackUtils::revealInFileManager(path)`, with a per-platform body:
`explorer /select,<path>` on Windows, `open -R <path>` on macOS, and on Linux the
`org.freedesktop.FileManager1.ShowItems` D-Bus call. If that fails or the platform has none of
these, it opens the containing folder with `QDesktopServices::openUrl`.

**Rationale**: Constitution Principle III asks for platform code behind a single interface with a
graceful fallback. Qt has no cross-platform "reveal" call.

## Open items

None. Every Technical Context item is resolved above.
