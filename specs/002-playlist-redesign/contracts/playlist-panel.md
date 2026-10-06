# Contract: Playlist Panel Behaviour

What the user can rely on. Covers FR-001 to FR-005, FR-008, FR-013, FR-019 to FR-025.

## Where it appears

| Situation | Presentation |
|---|---|
| Nothing loaded, or audio only | Fills the content area (as today). Always shown. |
| Video showing, panel closed | Not shown. |
| Video showing, panel open | Panel on the right edge over the video. The rest of the picture stays visible. |
| Window narrower than 560 px with video showing | Panel takes the full width and shows a close button in its header. The window's own minimum width is 640 px, so this happens only where the window manager makes it narrower (tiling, screen split). |
| Full screen | Same as windowed. The title bar and control bar do not auto-hide while the panel is open; they hide again after it closes. |
| Mini player | No playlist. Unchanged. |

Panel width: default 360 px, between 280 px and 60 % of the window width, changed by dragging the
panel's left edge. Width and open state are remembered across restarts.

## Opening and closing

| Action | Result |
|---|---|
| Playlist button in the control bar | Toggles the panel. |
| `Ctrl+L` | Toggles the panel. |
| `Ctrl+F` | Opens the panel if needed and puts the cursor in the search field. |
| Click on the visible part of the video while the panel is open | Closes the panel. Does not toggle play/pause. |
| `Esc` | Clears the search if it has text; otherwise closes the panel; otherwise leaves full screen (existing behaviour). |
| Starting an entry from the panel | Plays it. The panel stays open. |

Opening the panel scrolls the current entry into view. When playback moves to another entry, the
list scrolls only if the panel is open and the user is not interacting with it.

## Panel content, top to bottom

1. **Header**: title "Playlist" with the entry count, or "N of M" while a search or filter is
   active. Buttons: shuffle, save, collage, more (remove duplicates, clear, show channel logos). "Show channel
   logos" is the same setting as the switch under Settings → Playback → Playlist.
2. **Search row**: search field with a clear button; filter (All, Local files, Streams);
   sort (Playlist order, Title, Duration); group toggle, shown only when entries have groups.
3. **List**: entry rows and, when grouping is on, group header rows.
4. **Empty states**: "No media files" with how to add files, playlists or an address when the
   playlist is empty; "Nothing matches" with a "Clear search and filter" button otherwise.

## Rows

**Entry row** (three lines, fixed height):

- Leading: the channel logo when logos are on and the entry has one; otherwise an icon for the
  kind (video file, audio file, stream).
- Line 1: title, elided at the end.
- Line 2: the location, a local path or a web address, shortened in the middle when it does not
  fit.
- Line 3: kind in words ("Video", "Audio", "Stream"), then the duration when known, or "Live" for
  a stream that has been opened and found to be live, then the group when grouping is off. A
  stream that has not been opened shows only "Stream". For an unavailable entry, line 3 is
  "Unavailable" and the reason.
- Trailing: a "playing" indicator on the current entry; the queue position when queued; a remove
  button and a drag handle on hover or focus.
- An unavailable entry's title and icon are dimmed, still at 4.5:1 or better, and line 3 says so in
  words.
- No tooltip on the row: title and location are on the row itself.

**Group header row**: disclosure arrow, group name, entry count. Click or `Enter` toggles it.

## Mouse

| Action | Result |
|---|---|
| Click | Selects the row and gives the list keyboard focus. It no longer starts playback; double-click does. |
| Ctrl+click / Shift+click | Toggles / extends the selection. |
| Double-click | Plays the entry. |
| Right-click | Opens the row menu for the selection. |
| Drag the handle | Moves the selected entries. Disabled, with a tooltip explaining why, unless the view is in playlist order with no search, filter or grouping. |
| Drop files, a playlist or an address on the list | Inserts at the row under the pointer, shown by a line, without interrupting what is playing. In a searched, filtered, sorted or grouped view that is just before the entry under the pointer in playlist order. Files and playlists dropped together keep the order they were dropped in. A drop on a group heading, below the last row, or elsewhere in the window appends, as today. |

## Row menu

Play · Play next · Remove · Show in file manager (local files) or Copy address (streams).

"Play next" entries play in the order queued, before automatic next resumes, even if a search or
filter hides them by then. One that is unavailable when its turn comes is skipped (FR-022a).
With several entries selected: Play next (all, in order) · Remove.

## Keyboard (while the playlist has focus)

| Key | Result |
|---|---|
| `Up` / `Down` | Move focus one row and select it. With `Shift`, extend the selection instead. `Down` in the search field moves focus into the list. |
| `Page Up` / `Page Down` / `Home` / `End` | Move focus by a page or to the ends. |
| `Enter` | Play the focused entry, or toggle the focused group. |
| `Ctrl+Space` | Toggle selection of the focused row. |
| `Ctrl+A` | Select all visible entries. |
| `Delete` | Remove the selected entries, or the focused one when nothing is selected. |
| `Ctrl+Z` | Undo the last removal. |
| `Q` | Play the focused entry next. |
| `Alt+Up` / `Alt+Down` | Move the selected entries up or down one row, when reordering is allowed. |
| `Menu` or `Shift+F10` | Open the row menu. |
| `Left` / `Right` | Collapse / expand the focused group. |
| `Ctrl+F` or `/` | Focus the search field. |
| `Esc` | As in "Opening and closing". |

Global shortcuts (`Space`, `N`, `P`, `M`, …) do not fire while the search field has focus and
keep working when the list has focus. `Q` is new and only acts when the list has focus.

## Feedback

| Event | Message |
|---|---|
| Entries removed | "Removed N items" with an **Undo** action. |
| Duplicates removed | "Removed N duplicates" with **Undo**, or "No duplicates found". |
| Entries added while a search or filter hides them | "Added N items (hidden by the current search)", where N counts the hidden ones added in that action. |
| Playlist loaded with unavailable or unsupported entries | "Loaded N items, M unavailable", "Loaded N items, skipped M unsupported", or "Loaded N items, M unavailable, skipped K unsupported". |
| Reorder attempted while not allowed | Tooltip on the handle: "Reordering is available in playlist order with no search, filter or grouping". |

## Accessibility

- In both themes, text and icons have a contrast ratio of at least 4.5:1 against their background
  (3:1 for large text and icons) (FR-013).
- Every control has an accessible name. Rows announce title, kind and state ("playing",
  "unavailable", "queued").
- Kind and state are never conveyed by colour alone.
- Focus is always visible, in both themes.
