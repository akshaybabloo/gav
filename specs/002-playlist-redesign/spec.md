# Feature Specification: Playlist Redesign

**Feature Branch**: `002-playlist-redesign`

**Created**: 2026-10-05

**Status**: Clarified

**Input**: User description: "Redesign the playlist to better suit for both local and stream playing with better look and feel and better functionality"

## Background

Today the playlist is a plain list that covers the whole window. It appears when nothing is
playing or when the user toggles it, and hides the video while it is open. Each row shows an icon
and a name. There is a name search, a shuffle toggle, a clear button and a remove button per row.

That was enough for a handful of local files. It falls short now that GAV opens remote playlists
with thousands of channels: the user cannot browse the list while watching, the list ignores the
group and logo information such playlists carry, every row looks the same whether it is a file, a
live channel or something that failed to play, and there is no way to act on more than one item at
a time.

## Clarifications

### Session 2026-10-05

- Q: Where does the playlist sit while something is playing? → A: A panel drawn over one side of
  the video.
- Q: May GAV download channel logos from the addresses in the playlist? → A: Only when the user
  turns it on; off by default.
- Q: Are favourites in scope? → A: No, out of scope for this redesign.

### Session 2026-10-06

- Q: Should channel logos stay in this redesign, given that they break two constitution rules? →
  A: Yes. The constitution is amended first so that opt-in logos, with size and format limits and
  decoding on a background thread, are explicitly allowed.
- Q: How should a stream be labelled and filtered before GAV has opened it and learned whether it
  is live or on-demand? → A: Three kinds (local video, local audio, stream). A stream row says
  "Stream" until it has been opened, then shows "Live" or its duration. Filters are All, Local
  files and Streams; there is no Live filter.
- Q: What should Escape do when the panel is open and the search box has text? → A: Two steps: the
  first Escape clears the search text, the next closes the panel. With an empty search, one Escape
  closes it.
- Q: Should a "Play next" entry still play if a search or filter hides it, or if it is unavailable,
  when its turn comes? → A: Queued entries always play in the order queued, even when hidden.
  Unavailable ones are skipped.
- Q: What measurable bar should the playlist meet for readability and keyboard focus? → A: Text
  and icons meet a contrast ratio of at least 4.5:1 against their background in both themes (3:1
  for large text and icons), and every control shows a visible focus indicator when reached by
  keyboard.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Browse and switch while watching (Priority: P1)

A user is watching a video or a live channel and wants to see what else is in the playlist and
switch to another item without losing sight of what is playing.

**Why this priority**: This is the most common thing people do with a playlist, and today it
requires covering the video. It is also the foundation the other stories build on.

**Independent Test**: Play an item from a playlist of at least 20 entries, open the playlist, and
confirm the video keeps playing and stays visible while the list is browsed and another item is
started from it.

**Acceptance Scenarios**:

1. **Given** a video is playing, **When** the user opens the playlist, **Then** the playlist
   appears as a panel drawn over one side of the video, the rest of the picture stays visible, and
   playback continues without interruption.
2. **Given** the playlist is open while something plays, **When** the user starts another item
   from it, **Then** that item starts playing and the playlist stays open in the same place.
3. **Given** the playlist is open, **When** the user looks at it, **Then** the item that is playing
   is clearly marked and is scrolled into view when the playlist opens.
4. **Given** the user has opened, closed or resized the playlist, **When** they restart GAV,
   **Then** the playlist comes back in the same state and size.
5. **Given** nothing is loaded, **When** GAV starts, **Then** the playlist area shows a clear empty
   state that explains how to add files, playlists or a web address.
6. **Given** the window is in full screen or too narrow for the panel to leave part of the picture
   visible, **When** the user opens the playlist, **Then** it is still reachable and can be
   dismissed with one action, and the video is never left permanently covered.
7. **Given** the playlist panel is open over a playing video, **When** the user clicks the visible
   part of the picture, or presses Escape with an empty search box, **Then** the panel closes and
   playback continues.
8. **Given** the panel is open and the search box has text, **When** the user presses Escape,
   **Then** the search is cleared and the panel stays open. A second Escape closes it.

---

### User Story 2 - Rows that tell you what each item is (Priority: P2)

A user scanning the playlist wants to tell at a glance what each entry is: a local video, a local
audio file or a stream, how long it is when that is known, and whether it can be played.

**Why this priority**: A better look and feel is half of the request, and mixed local and stream
playlists are unreadable when every row looks the same.

**Independent Test**: Load a playlist that mixes local video, local audio, streams and a missing
file, and confirm local video, local audio, streams and the unavailable entry are distinguishable
without playing anything. Then open one on-demand stream and one live stream and confirm their
rows change to a duration and to "Live".

**Acceptance Scenarios**:

1. **Given** a mixed playlist, **When** the user views it, **Then** each row shows a title and a
   second line with what is known about the item: its kind, its duration when known, and its group
   when the playlist provides one. A stream that has not been opened says "Stream".
   Once a stream has been opened, its row shows its duration if it is on-demand or "Live" if it
   is live.
2. **Given** an item's duration is not known when it is added, **When** it becomes known (for
   example after the item has been opened), **Then** the row updates without the user doing
   anything.
3. **Given** an item failed to play or its file is missing, **When** the user views the playlist,
   **Then** the row is marked as unavailable with the reason available on demand, and the item is
   skipped by automatic next/previous.
4. **Given** a title is too long for the row, **When** the user points at it, **Then** the full
   title and the item's location are shown.
5. **Given** a playlist provides an image for an entry and the user has turned channel logos on,
   **When** the row is shown, **Then** the image appears next to the title. With the setting off,
   which is the default, no image is requested and the row shows its kind icon.
6. **Given** the light or dark theme, **When** the playlist is shown, **Then** it follows the
   theme, text and icons meet the contrast ratios in FR-013, and the focused control is visibly
   marked.

---

### User Story 3 - Find things in very large playlists (Priority: P3)

A user has opened a channel list with more than ten thousand entries and wants to reach one
channel, or one group of channels, quickly.

**Why this priority**: Remote playlists of this size are now supported, and without finding tools
they are unusable. Local users with a few files do not need this, so it ranks below the first two.

**Independent Test**: Open a playlist with at least 10,000 entries that carries group information
and confirm a named channel can be found and started in a few seconds, by search and by group.

**Acceptance Scenarios**:

1. **Given** a playlist of 10,000 entries, **When** the user types in the search box, **Then** the
   list narrows as they type to entries whose title or group matches, and shows how many matched.
2. **Given** entries carry group information, **When** the user turns grouping on, **Then** the
   list is organised under group headings that can be collapsed and expanded, each showing how
   many entries it holds.
3. **Given** a mixed playlist, **When** the user chooses a filter, **Then** only local files or
   only streams are shown.
4. **Given** any playlist, **When** the user chooses a sort order (playlist order, title, or
   duration), **Then** the list is shown in that order without changing the saved playlist order.
5. **Given** a search or filter that matches nothing, **When** the list is empty, **Then** a
   message says so and offers to clear the search or filter.
6. **Given** a search, filter, grouping or sort is active, **When** the current item ends,
   **Then** the next item played is the next one in what the user is looking at.

---

### User Story 4 - Manage several items at once (Priority: P4)

A user wants to tidy a playlist: reorder entries, remove a batch, or queue something to play next.

**Why this priority**: These are the functions people expect from a playlist and the current one
only removes single rows. They matter less than seeing and finding items.

**Independent Test**: With a playlist of 10 items, select three, remove them, drag another to a new
position, queue one to play next, and confirm the playlist and the playback order reflect each
change.

**Acceptance Scenarios**:

1. **Given** a playlist, **When** the user selects several rows with the mouse or keyboard,
   **Then** they can remove them in one action, with a single undo.
2. **Given** a playlist shown in playlist order, **When** the user drags one or more rows to a new
   position, **Then** the order changes and is kept when the playlist is saved.
3. **Given** a row, **When** the user opens its menu, **Then** they can play it, play it next,
   remove it, and either show the file in the system's file manager (local files) or copy its
   address (streams).
4. **Given** files, a playlist file or a web address are dropped onto the playlist, **When** the
   drop lands between two rows, **Then** the new items are inserted at that position.
5. **Given** the playlist has focus, **When** the user uses the keyboard, **Then** they can move
   through rows, play the focused row, remove the selection and jump to the search box without the
   mouse.
6. **Given** a playlist with more than one entry for the same location, **When** the user asks to
   remove duplicates, **Then** only the first of each is kept and the user is told how many were
   removed.

---

### Edge Cases

- An empty playlist, and a playlist with a single entry.
- A playlist of more than 10,000 entries: opening it, scrolling it, searching it and clearing it
  must not freeze the window.
- Entries with no title, with the same title, or with the same location.
- Group names that are empty, very long, or shared by thousands of entries.
- A local file that is moved or deleted after it was added.
- A stream that stops responding, or a live channel that ends.
- Reordering while a search, filter, grouping or non-default sort is active: reordering is only
  offered in playlist order, and the user is told why it is unavailable otherwise.
- Shuffle and repeat while a filter is active.
- An entry queued with "play next" that is later hidden by a search or filter still plays; one
  that has become unavailable is skipped.
- Removing the item that is playing: playback of that item continues until it ends or the user
  starts another.
- The mini player and full screen: the playlist must not break either.
- Restoring the last session's playlist, and saving a playlist after it was reordered or filtered:
  saving always writes every entry in playlist order, not only the visible ones.
- Items added while the playlist is searched or filtered and do not match: the user is told they
  were added.

## Requirements *(mandatory)*

### Functional Requirements

**Layout and presence**

- **FR-001**: Users MUST be able to open and close the playlist at any time, including while
  something is playing, with one action from the controls and one keyboard shortcut.
- **FR-002**: While something is playing, the playlist MUST be presented as a panel drawn over one
  side of the video, leaving the rest of the picture visible. Playback MUST NOT be interrupted by
  opening, closing or using it. When nothing is playing, the playlist fills the content area as it
  does today.
- **FR-002a**: Clicking the visible part of the picture MUST close the panel without affecting
  playback. Pressing Escape MUST clear the search text if there is any, and otherwise close the
  panel, also without affecting playback.
- **FR-003**: The system MUST remember whether the playlist was open and how large it was, and
  restore that on the next start.
- **FR-004**: The playlist MUST remain usable in full screen and in narrow windows, and MUST be
  dismissible with one action in both (two presses of Escape when a search is active, as in
  FR-002a).
- **FR-005**: The system MUST show an empty state that explains how to add files, playlists and
  web addresses when the playlist has no entries.

**Rows**

- **FR-006**: Each row MUST show the entry's title, its location (file path or web address) and a
  line with the information known about it: kind, duration when known, "Live" for a stream known
  to be live, and group.
- **FR-007**: Each row MUST show, without relying on colour alone, whether the entry is a local
  video, a local audio file or a stream. Whether a stream is live or on-demand is not known until
  it has been opened: until then its row says "Stream", and afterwards it shows "Live" or its
  duration.
- **FR-008**: The entry that is playing MUST be clearly marked, and MUST be scrolled into view when
  the playlist opens and when playback moves to another entry.
- **FR-009**: Entries that failed to play or whose file is missing MUST be marked as unavailable,
  MUST show the reason on demand, and MUST be skipped by automatic next and previous.
- **FR-010**: Information that becomes known later (duration, live or on-demand, availability) MUST
  appear on the row without user action.
- **FR-011**: The title and location of an entry MUST be shown on its row. A location too long for
  the row is shortened in the middle so that its start and its file name stay visible; widening
  the playlist shows more of both. They are not shown in a hover tooltip.
- **FR-012**: When a playlist provides an image for an entry, the system MUST show it on the row
  only if the user has turned channel logos on. The setting MUST be off by default. With it off,
  no image is requested.
- **FR-012a**: With channel logos on, images MUST be requested only for rows the user can see or
  is about to scroll to, only from `http` and `https` addresses, and with a time limit and a size
  limit per image. Only common still-image formats are accepted, with a limit on pixel dimensions,
  and decoding MUST NOT block the interface. An image that fails or exceeds a limit falls back to
  the kind icon without an error message.
- **FR-012b**: Downloaded images MUST be kept only in a cache that the user can clear, and turning
  the setting off MUST stop further requests.
- **FR-013**: The playlist MUST follow the light and dark themes. In both, text and icons MUST
  have a contrast ratio of at least 4.5:1 against their background (3:1 for large text and icons),
  and every control MUST show a visible focus indicator when reached by keyboard.

**Finding**

- **FR-014**: Users MUST be able to search the playlist by title and group, with results updating
  as they type and a count of matches shown.
- **FR-015**: Users MUST be able to group entries by the group information a playlist provides,
  and collapse and expand groups. Entries without a group MUST appear under a clearly named
  catch-all.
- **FR-016**: Users MUST be able to filter the playlist to local files or to streams. There is no
  separate filter for live streams.
- **FR-017**: Users MUST be able to sort the view by playlist order, title or duration without
  changing the playlist's own order.
- **FR-018**: Automatic next, previous and shuffle MUST follow what the user currently sees:
  entries hidden by search or filter are not played automatically. Entries the user queued with
  "play next" are the exception (FR-022a).
- **FR-019**: The system MUST show a message with a way to clear the search or filter when nothing
  matches.

**Managing**

- **FR-020**: Users MUST be able to select several entries and remove them in one action, and undo
  that removal once.
- **FR-021**: Users MUST be able to reorder one or more entries by dragging when the view is in
  playlist order with no search or filter, and the new order MUST be kept when the playlist is
  saved or restored.
- **FR-022**: Each entry MUST offer: play, play next, remove, and either show in the file manager
  (local files) or copy address (streams).
- **FR-022a**: Entries queued with "play next" MUST play in the order they were queued, before
  automatic next resumes, even if a search or filter hides them by then. A queued entry that is
  unavailable when its turn comes MUST be skipped.
- **FR-023**: Items dropped onto the playlist MUST be inserted where they are dropped. Items added
  any other way are added at the end.
- **FR-024**: Users MUST be able to remove duplicate entries, keeping the first of each, and be
  told how many were removed.
- **FR-025**: Every playlist action MUST be reachable from the keyboard.
- **FR-026**: Existing playlist functions MUST keep working: open and save playlist files, clear,
  shuffle, repeat modes, collage from the playlist, and restoring the last session's playlist.
- **FR-027**: Saving a playlist MUST write every entry in playlist order, together with the title
  and group of each entry, regardless of the current search, filter, grouping or sort.

**Scale and safety**

- **FR-028**: Opening, scrolling, searching, grouping, sorting and clearing a playlist of 10,000
  entries MUST NOT make the window unresponsive.
- **FR-029**: The playlist MUST NOT cause any network request by itself, apart from channel logos
  when the user has turned them on (FR-012). Listing, searching, grouping and sorting use only
  what is already known about the entries.

### Key Entities

- **Playlist entry**: one thing that can be played. It has a location (a local file or a web
  address), a title, a kind (local video, local audio, stream), for streams whether it is live,
  on-demand or not yet known, an optional group, an optional image address, an optional duration,
  and an availability state (unknown, playable, unavailable with a reason).
- **Group**: a named set of entries taken from the playlist's own group information. It has a name,
  a count, and a collapsed or expanded state in the view.
- **Playlist view**: how the user is currently looking at the playlist. It has a search text, a
  filter, a sort order, whether grouping is on, the selection, and the open or closed state and
  size of the playlist. It never changes the playlist's own order.
- **Play queue**: the entries the user asked to play next, played in the order queued before
  automatic next resumes, whether or not the current search or filter shows them.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: While watching, a user can switch to another playlist entry in at most two actions
  without the picture being fully hidden at any point. At the default window size, at least half
  of the picture stays visible while the panel is open.
- **SC-002**: In a playlist of 10,000 entries, a user can find and start a named channel in under
  10 seconds using search.
- **SC-003**: With 10,000 entries, the list responds to each typed search character, scroll, sort
  or group change within 200 milliseconds, and the window never stops responding for longer than
  that.
- **SC-004**: Opening a 10,000-entry playlist shows the first rows within 2 seconds of the
  playlist being read.
- **SC-005**: A first-time user can tell a local video, a local audio file, a stream and an
  unavailable entry apart in a mixed playlist without playing any of them, and can tell a live
  stream from an on-demand one for every stream that has been opened once.
- **SC-006**: Removing 50 selected entries, or moving 10 entries to a new position, takes one
  action each.
- **SC-007**: Every playlist action can be completed with the keyboard alone.
- **SC-008**: Everything that worked with the old playlist (open, save, clear, shuffle, repeat,
  collage, session restore) still works, with no entries lost or reordered when an existing
  playlist file is opened and saved again.

## Assumptions

- The panel opens on the right side of the window. Which side is not configurable in this
  redesign.
- The redesign changes how the playlist looks and what can be done in it. It does not change which
  files and addresses GAV can play.
- Group, title and image information comes only from what the playlist file or the media already
  provides. GAV does not look anything up, and programme guides are out of scope.
- Durations are shown when they are already known or become known through normal use. GAV does
  not open every entry in advance to measure it.
- Channel logos depend on a constitution amendment that explicitly allows opt-in logo downloads
  and in-app decoding of small still images within the limits in FR-012a. No logo work starts
  before that amendment is made.
- Favourites are out of scope for this redesign and can be specified as their own feature later.
- Channel logos are the only images shown. Thumbnails generated from local videos are out of scope. Local entries are told apart by kind
  and duration.
- "Play next" lasts for the current session and is not saved with the playlist.
- Search, filter, grouping and sort choices are remembered for the session; the open or closed
  state and size of the playlist are remembered across restarts.
- The mini player keeps its current behaviour and does not get its own playlist.
- Existing settings that affect the playlist (shuffle, restore last playlist on startup) keep their
  meaning and defaults.
