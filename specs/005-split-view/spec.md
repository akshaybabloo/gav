# Feature Specification: Split View for up to Four Videos

**Feature Branch**: `205-split-view`

**Created**: 2026-10-21

**Status**: Draft

**Input**: User description: "See gh 205 for more information" (issue #205, "Play multiple videos - split")

## Background

GAV plays one video at a time. Issue #205 asks for up to four videos playing at once, side by side
in one window, with sound from only one of them at a time.

The clearest use is watching several live sources together, such as the cameras that GAV can play
since RTSP support was added: a wall of up to four cameras, listening to one. It also serves
comparing videos, or keeping an eye on one while watching another.

Decided in the issue:

- Up to four videos play at the same time in a split view.
- Clicking a video selects it: only that one is unmuted, the rest are muted. The top-left video is
  selected to begin with.
- Double-clicking a video makes it fill the grid and pauses the others. A second double-click then
  goes full screen.
- Coming back to the grid puts the video back in its place, and the others start playing again,
  muted.

The issue also proposed how to get back from full screen, the layouts, what the controls act on,
how live streams behave while another video fills the grid, and a performance target. This
specification takes those proposals as its starting point.

## Clarifications

### Session 2026-10-21

- Q: During a split view, what does a usual open (File > Open, Open URL, the command line, Play in the playlist) do, and can files be dropped into the split view? → A: A usual open replaces the selected tile's video. Dropping a file onto a tile replaces that tile's video; dropping it onto empty space in the video area adds a tile.
- Q: What does a live stream in the split view do while another video fills the area? → A: It keeps playing, muted and out of sight, so it is still live when the grid comes back. Only files pause.
- Q: What does a tile do when its video ends, fails or is lost, and what do next and previous do? → A: The tile keeps its place, showing the end or the failure, until it is closed or replaced. Next and previous change only the selected tile, skipping entries already in the grid.
- Q: When one video is already playing on its own, how does the user put a second video beside it? → A: The playlist's context menu has "Play in split view" for any entry or selection. A video playing alone stays as the first tile and the chosen entries join it; in a split view the chosen entries are added as tiles, up to four.
- Q: In a split view, what does the Stop button do to the selected video, and how does the user close a tile? → A: Stop closes the selected tile: its video stops and the grid rearranges. There is no separate close button on a tile.
- Q: When the playlist panel is opened during a split view, does it cover part of the grid or does the grid shrink to make room? → A: It covers the right side of the grid, exactly as it covers a single video today. The grid keeps its full size.
- Q: While one video fills the area after a double-click, what does pressing 1, 2, 3 or 4 do? → A: The video in that position fills the area instead and becomes selected and heard. The one it replaces goes out of sight, paused if it is a file and still playing if it is live. Full screen, if on, stays on.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Watch up to four videos at once (Priority: P1)

A user has several videos or streams in the playlist. They select two, three or four of them and
play them in a split view. All of them play at the same time in one window, arranged in a grid.
The top-left one is selected and is the only one heard. If a video is already playing on its own,
choosing "Play in split view" for one or more other entries keeps it playing and puts them beside
it.

**Why this priority**: This is the feature. Selecting, focusing and full screen all need videos in
a grid first.

**Independent Test**: Add four local videos to the playlist, select them, choose to play them in a
split view, and confirm that all four play at once in a 2×2 grid with only the top-left one
audible.

**Acceptance Scenarios**:

1. **Given** nothing is playing and four videos are selected, **When** the user chooses to play them in a
   split view, **Then** all four play at the same time in a 2×2 grid, in playlist order from the
   top-left, and only the top-left one is heard.
2. **Given** two videos are selected, **When** the user plays them in a split view, **Then** they
   play side by side, each half the width of the video area.
3. **Given** three videos are selected, **When** the user plays them in a split view, **Then** they
   fill three cells of a 2×2 grid and the fourth cell is empty.
4. **Given** the chosen entries, together with any video already playing, would make more than
   four, **When** the user looks for the split view choice, **Then** it is unavailable and says
   that a split view holds up to four videos.
5. **Given** one video is playing alone, **When** the user chooses "Play in split view" for
   another entry, **Then** the playing video carries on without restarting as the left tile and
   the chosen entry plays beside it, muted; the playing video stays selected.
6. **Given** nothing is playing and a single entry is chosen, **When** the user looks for the
   split view choice, **Then** it is unavailable: one video plays in the normal single view.
7. **Given** a split view is playing, **When** the user stops tiles until one video is left,
   **Then** that video carries on playing in the normal single view, with its sound on.
8. **Given** a split view is playing, **When** the user ends the split view from the menu, **Then**
   the selected video carries on in the normal single view and the others stop.

---

### User Story 2 - Choose which video is heard and controlled (Priority: P1)

The user clicks a video in the grid. It becomes the selected video: it is the only one heard, it
is marked so the user can see which one it is, and the control bar, the keyboard and the system's
media keys act on it.

**Why this priority**: Four videos with four sounds at once is noise, and controls that act on an
unknown video are unusable. A split view is not usable without this, so it shares P1.

**Independent Test**: In a split view of four videos, click each in turn and confirm that the
sound moves to it, the mark moves to it, and pausing, seeking and changing the audio track affect
only that video.

**Acceptance Scenarios**:

1. **Given** a split view with the top-left video selected, **When** the user clicks the
   bottom-right video, **Then** the bottom-right video is heard, the top-left is muted, and the
   mark moves to the bottom-right video.
2. **Given** a video is selected, **When** the user pauses, seeks, changes speed, picks an audio or
   subtitle track, picks a quality, zooms, adjusts brightness or contrast, sets A-B repeat or takes
   a screenshot, **Then** only the selected video is affected.
3. **Given** a split view, **When** the user presses 1, 2, 3 or 4, **Then** the video in that
   position (left to right, top to bottom) becomes the selected one.
4. **Given** a video is selected, **When** the user looks at the window title, the seek bar, the
   statistics overlay and the system's now-playing display, **Then** each shows the selected video.
5. **Given** the selected video is muted with the mute control, **When** the user selects another
   video, **Then** no video is heard until the user unmutes, and unmuting makes the newly selected
   one heard.

---

### User Story 3 - Focus one video, then full screen (Priority: P2)

The user double-clicks a video in the grid. It fills the grid, the other videos that are files
pause, and live streams carry on out of sight. Double-clicking it again takes it full screen. The
user can come back to the grid, where every video is in its old place and the paused ones play on
from where they paused, muted.

**Why this priority**: Decided in the issue and useful for looking closely at one source, but the
grid is usable without it.

**Independent Test**: In a split view of four local videos, double-click one, confirm it fills the
grid and the others are paused; double-click again and confirm full screen; come back and confirm
the grid is as it was and the others continue from where they paused.

**Acceptance Scenarios**:

1. **Given** a split view, **When** the user double-clicks a video, **Then** that video fills the
   video area, becomes the selected video, and the other videos that are files pause.
2. **Given** a video fills the grid, **When** the user double-clicks it again, **Then** the window
   goes full screen showing that video.
3. **Given** a focused video is full screen, **When** the user double-clicks it, **Then** the window
   leaves full screen and the grid comes back with every video in its old place, and the others
   play on, muted, from where they paused.
4. **Given** a focused video is full screen, **When** the user presses Esc, **Then** the window
   leaves full screen and the video still fills the grid.
5. **Given** a video fills the grid and the window is not full screen, **When** the user presses
   Esc, **Then** the grid comes back as in scenario 3.
6. **Given** a split view where one of the other videos is a live stream, **When** the user focuses
   a different video, **Then** the live stream keeps playing, muted and out of sight, and is still
   live when the grid comes back.
7. **Given** a single video plays in the normal single view, **When** the user double-clicks it,
   **Then** it toggles full screen as it does today.
8. **Given** the video in position 1 fills the area, full screen or not, **When** the user presses
   3, **Then** the video in position 3 fills the area instead, is selected and heard, and plays on
   from where it paused if it is a file; the video in position 1 goes out of sight, paused if it
   is a file. Full screen stays as it was.

---

### User Story 4 - Change what is in the split view (Priority: P3)

While a split view is playing, the user replaces one of the videos, adds one, or closes one, and
the grid rearranges to fit.

**Why this priority**: Starting a split view from the playlist covers the main use; changing it in
place saves starting over.

**Independent Test**: In a split view of two videos, drop a third file onto the video area and
confirm it joins the grid; drop a file onto one tile and confirm it replaces that tile's video;
select a tile and press Stop, and confirm that the tile closes and the grid rearranges.

**Acceptance Scenarios**:

1. **Given** a split view of fewer than four videos, **When** the user chooses "Play in split view"
   for one or more entries, or drops a file onto empty space in the video area, **Then** the new
   videos join the grid in the next free positions, muted, the layout grows to fit, and the
   selection does not change.
2. **Given** a split view, **When** the user presses Stop (on the control bar or with the
   system's media keys), **Then** the selected tile closes: its video stops, the
   remaining videos keep their order and move up to fill the gap, and the top-left video becomes
   selected.
3. **Given** a split view of four videos, **When** the user tries to add a fifth, **Then** nothing
   is added and GAV says that a split view holds up to four videos.
4. **Given** a split view, **When** the user plays an entry that is already in the grid, **Then**
   that entry's tile becomes selected and nothing restarts.
5. **Given** a split view, **When** the user opens or plays something the usual way (File > Open,
   Open URL, the command line, or Play in the playlist), **Then** it replaces the selected tile's
   video, in the same place, and the split view carries on.
6. **Given** a split view, **When** the user drops a file onto a tile, **Then** it replaces that
   tile's video.
7. **Given** a split view of fewer than four videos, **When** the user drops a file onto empty
   space in the video area, **Then** it joins the grid as in scenario 1.
8. **Given** one video playing, **When** the user drops a video file onto it, **Then** GAV asks
   whether to play it in a split view or replace the playing video; Play in split view adds it
   beside the playing video, Replace replaces it, and Cancel changes nothing.
9. **Given** a split view, **When** the user presses next or previous, **Then** only the selected
   tile changes, to the next or previous playlist entry that is not already in the grid.

---

### Edge Cases

- A video in a tile reaches its end, or a stream in a tile fails or is lost: the tile keeps its
  place, showing the end or the failure, until it is closed or replaced. Any failure message names
  that video. The other tiles do not move.
- An audio-only file is put in a tile: the tile shows what the single view shows for audio today,
  and the file is heard only when its tile is selected.
- A camera fails while another tile's file or HTTP source is opening and failing: the camera may
  show the general "could not open" message, never the other source's reason.
- A file in a tile cannot be opened: the failure message names that file, and the other videos
  carry on.
- Several RTSP cameras open at the same moment, with Automatic transport: each finds its own
  transport, and a sign-in or "not found" failure is reported for the camera it belongs to and no
  other.
- The window is made very small: every tile stays visible, however small; nothing overlaps.
- The machine cannot keep up with four videos: the videos drop frames as they would alone, the
  window and controls stay responsive, and GAV does not stop any video by itself.
- A video with a saved resume position is put in a split view while "remember positions" is on:
  it starts from the beginning without the resume prompt, and its position is saved as usual when
  it stops. The prompt appears only in the single view.
- The same entry is chosen twice: it is shown in one tile only.
- The playlist is cleared, or entries in the grid are removed from it, while the split view plays:
  the removed entries' tiles close as in User Story 4.
- The playlist panel is open over a split view: tiles under it are hidden until the panel is
  closed or narrowed. Clicks on the panel never select the tile underneath.
- The mini player is asked for during a split view: it is unavailable while more than one video is
  playing.
- GAV is closed during a split view: on the next start the split view is not restored. The
  playlist is restored as today when that setting is on.

## Requirements *(mandatory)*

### Functional Requirements

**Starting and ending a split view**

- **FR-001**: The playlist's context menu MUST offer "Play in split view" for one entry or a
  selection. With nothing playing, it MUST start a split view of the chosen entries; it is
  unavailable for a single entry. With one video playing alone, that video MUST carry on without
  restarting as the first tile and stay selected, and the chosen entries MUST join it. In a split
  view, the chosen entries MUST be added as tiles. Entries already in the grid MUST be left out of
  the count and not added again.
- **FR-002**: A split view MUST hold at most four videos. Any action that would add a fifth MUST be
  unavailable or refused with a message saying that a split view holds up to four videos.
- **FR-003**: The layout MUST follow the number of videos: one video fills the video area, two sit
  side by side, three or four use a 2×2 grid. Each video MUST keep its own picture shape inside its
  cell.
- **FR-003a**: The playlist panel MUST sit over the right side of a split view exactly as it sits
  over a single video today; the grid MUST keep its full size underneath. Choosing "Play in split
  view" from the panel MUST leave the panel open or closed as it was, as playing another entry
  from the playlist does today.
- **FR-004**: Tiles MUST be filled left to right and top to bottom. Videos already in the split view
  (or the video that was playing alone) keep their positions; newly added entries take the next
  free positions in playlist order.
- **FR-005**: In a split view, Stop MUST close the selected tile: its video stops and the grid
  rearranges as in FR-004, keeping the order of the others. A tile MUST have no close button of
  its own; to close another tile the user selects it and presses Stop. Users MUST also be able to
  end the whole split view. When one video is left, it MUST carry on in the normal single view,
  where Stop behaves as it does today. Stop while a video fills the area MUST close that tile,
  bring the grid back with the others, and resume the files the focus paused.
- **FR-006**: The way into the split view MUST be a menu choice or a gesture, not a permanent
  button on the playback view (Constitution I).
- **FR-007**: During a split view, a usual open (File > Open, Open URL, the command line, Play in
  the playlist) MUST replace the selected tile's video, in the same place. Dropping a file onto a
  tile MUST replace that tile's video; dropping it onto empty space in the video area MUST add a
  tile (FR-002 applies). An entry already in the grid MUST NOT be opened a second time: its tile
  becomes selected instead. With one video playing, dropping a media file onto the video area
  MUST ask whether to play it in a split view or replace the playing video, every time; Cancel
  MUST change nothing. A drop of only subtitle or playlist files MUST NOT ask.

**Sound and selection**

- **FR-008**: Exactly one video in a split view MUST be the selected one. The top-left video MUST be
  selected when a split view starts.
- **FR-009**: Only the selected video MAY be heard. Every other video MUST be silent at all times.
- **FR-010**: Clicking a video MUST select it. Pressing 1 to 4 MUST select the video in that
  position. While a video fills the area, pressing 1 to 4 MUST make the video in that position
  fill the area instead, selected and heard, playing on from where it paused if it is a file;
  FR-017 and FR-018 apply to the one it replaces, and full screen MUST stay as it was. A key for a
  position with no video MUST do nothing.
- **FR-011**: The selected video MUST be marked by a 2-pixel border in the theme's accent colour,
  drawn only in a split view, in light and dark themes.
- **FR-011a**: Each tile MUST tell assistive technology its position and title, and whether it is
  the selected tile.
- **FR-012**: The volume and mute controls MUST apply to what is heard: changing the selection MUST
  keep the volume and mute state.
- **FR-013**: The control bar, keyboard shortcuts and the system's media keys MUST act on the
  selected video only: play and pause, stop (FR-005), seek, go to time, frame step, speed, repeat
  and A-B repeat, audio and subtitle tracks, quality, zoom and pan, brightness and contrast,
  screenshot, and the statistics overlay.
- **FR-014**: Each video MUST keep its own settings from the list in FR-013 while another one is
  selected, and the controls MUST show the selected video's settings when the selection changes.
- **FR-015**: The window title, the seek bar, the statistics overlay and the system's now-playing
  display MUST show the selected video. The playlist MUST show which entries are in the split view
  and which one is selected.
- **FR-016**: Each video MUST show its own subtitles, whether or not it is selected.

**Focus and full screen**

- **FR-017**: Double-clicking a video in the grid MUST make it fill the video area, select it, and
  pause the other videos that are files.
- **FR-018**: While another video fills the area, a live stream in the split view MUST keep
  playing, muted and out of sight, so that it is still live when the grid comes back.
- **FR-019**: Double-clicking the focused video MUST take the window full screen. Double-clicking it
  in full screen MUST leave full screen and bring the grid back.
- **FR-020**: Esc MUST leave full screen and keep the focused video filling the area. Esc while a
  video fills the area outside full screen MUST bring the grid back.
- **FR-021**: When the grid comes back, every video MUST be in the place it had, and the videos
  the focus paused MUST play on from where they paused, muted unless selected; a video the user
  had paused stays paused.
- **FR-022**: Outside a split view, double-click MUST keep toggling full screen as it does today.
- **FR-023**: In the grid (no video focused), the full-screen shortcut MUST take the whole grid full
  screen. While a video fills the area, it MUST toggle full screen and keep that video filling the
  area.

**Each video's life**

- **FR-024**: When a tile's video ends, fails or is lost, the tile MUST keep its place, showing the
  end or the failure, until the user closes or replaces it. The other tiles MUST NOT move.
- **FR-024a**: Next and previous MUST change only the selected tile, to the next or previous
  playlist entry that is not already in the grid. Automatic advance to the next entry at the end
  of a video MUST NOT happen in a split view.
- **FR-025**: A failure message for a video in a split view MUST name that video, and MUST NOT stop
  or pause the others. Such a message MUST start with the entry's title in its display form, for
  example "Camera 2: The stream was lost."
- **FR-026**: Every kind of source GAV plays MUST work in a tile: local video and audio files, HTTP
  and HLS streams, and RTSP streams, each with the behaviour it has in the single view. Each tile
  MUST show its own buffering, "Reconnecting…" and failure state on the tile, whether or not it is
  selected. The live badge and the quality menu belong to the control bar and follow the selected
  tile.
- **FR-027**: Several RTSP streams opening at the same time MUST each use the transport chosen for
  it by the Settings value and the session's memory of that source, and each MUST receive only its
  own failure kind.
- **FR-028**: No resume prompt MUST appear for videos started in a split view; they start from the
  beginning. Positions MUST be saved for them as in the single view when "remember positions" is on.
- **FR-029**: The mini player MUST be unavailable while more than one video is playing.
- **FR-030**: The split view MUST NOT be saved or restored between sessions.

**Unchanged**

- **FR-031**: With one video playing, GAV MUST behave exactly as before this feature, except for
  the drop question in FR-007.
- **FR-032**: Opening a split view MUST NOT make any network request beyond those each of its
  sources makes when played alone, and closing a tile or the split view MUST end that tile's
  requests (Constitution I).

### Key Entities

- **Split view**: what the video area shows when more than one video plays. It has two to four
  tiles in a fixed order, a layout derived from the count, one selected tile, and at most one
  focused tile (filling the area).
- **Tile**: one video in the split view. It refers to one playlist entry and has its own playback
  state, position, tracks, subtitles, quality, speed, repeat settings, zoom and pan, and picture
  adjustments.
- **Selected tile**: the one tile that is heard and that every control acts on.
- **Focused tile**: the tile that fills the area after a double-click, while the others wait.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Once four playlist entries are selected, one menu choice starts all four playing in a
  split view, and for local files all four are playing within 5 seconds.
- **SC-002**: Selecting a video moves the sound to it within 0.3 seconds, and at no time are two
  videos heard together.
- **SC-003**: Four 1080p videos at 30 frames per second play together for 10 minutes with less than
  1% of frames dropped in each, on a machine with hardware video decoding. The window responds to
  clicks and keys within 100 milliseconds throughout.
- **SC-004**: Focusing a video and coming back to the grid restores every video to its old place,
  and every file the focus paused continues within 1 second of where it paused.
- **SC-005**: Four RTSP cameras on a local network, opened together, are all playing within 10
  seconds, each with the transport it needs, and every failure message names the camera it
  belongs to.
- **SC-006**: Every control listed in FR-013 changes only the selected video, checked control by
  control.
- **SC-007**: All existing automated tests pass, and the single-video checks of earlier features
  give the same results as before.

## Assumptions

- **Proposed in the issue and taken here**: the layouts for one to four videos; the selected video
  has a visible mark and is what the control bar acts on; keys 1 to 4 select a video; a third
  double-click leaves full screen and returns to the grid, and Esc leaves full screen while the
  video still fills the grid; the performance target is four 1080p videos.
- The issue's "pauses the others" applies to files; live streams keep playing out of sight
  (clarified on 2026-10-21).
- Esc while a video fills the grid outside full screen returns to the grid: the issue gives that
  state no other way back that does not pass through full screen.
- The digit keys 1 to 4 have no other meaning in GAV today.
- GAV has no Stop key. Closing a tile uses the control bar or the media keys.
- There is one volume setting for the window, applied to whichever video is heard.
- A View menu choice ends the split view. The playlist's existing multi-selection is what "Play
  in split view" acts on.
- Resume prompts stay a single-view feature: four prompts at once would block the split view.
- The mini player stays a single-video feature in this version.
- Split views are not restored between sessions; the last-session playlist is restored as today.
- Four 4K videos decoded without hardware help are not expected to play smoothly; the success
  criteria use 1080p.
- Out of scope: more than four videos, layouts the user arranges by hand, tiles of different
  sizes, synchronised playback across tiles, mixing the sound of several tiles, and a separate
  playlist per tile.
