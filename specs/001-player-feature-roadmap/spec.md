# Feature Specification: Player Feature Roadmap

**Feature Branch**: `main` (no feature branch created)

**Created**: 2026-10-02

**Status**: Draft

**Input**: User description: "plan for what more features can be added to the application"

## Overview

GAV already covers core playback: play/pause, seeking with hover previews, volume, playback speed, brightness/contrast, zoom, repeat modes (once, loop, A-B range), a mini player, a filterable drag-and-drop playlist, frame capture, a stats overlay, update checks, single-instance file hand-off and full-screen with auto-hiding cursor. It also produces thumbnail collages from the command line.

This roadmap covers the four largest remaining gaps compared with everyday desktop media players. Each area is written as an independently deliverable user story, in priority order, so that it can be planned and shipped on its own.

## Clarifications

### Session 2026-10-02

- Q: Should this spec be planned as one plan covering all four stories, or cut down? → A: Keep all four stories in this spec and plan them together.
- Q: How closely should GAV reproduce `.ass`/`.ssa` subtitle styling? → A: Full fidelity: fonts, exact positioning, rotation, karaoke and animated effects, matching dedicated players.
- Q: When reopening a partly watched file, should GAV resume automatically, ask first, or start over? → A: Ask first ("Resume from 12:34?" with Resume / Start over), waiting for a choice before playing.
- Q: Should GAV restore the last session's playlist on startup? → A: Yes, as a setting that is off by default.
- Q: Which player's keyboard layout should new shortcuts follow? → A: VLC-style keys throughout, keeping GAV's existing bindings.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Subtitles and track selection (Priority: P1)

A viewer opens a foreign-language film or a video with multiple audio dubs. They want to see subtitles, whether the subtitles are inside the video file or in a separate file next to it, and switch to the audio language they prefer. If the subtitles are out of sync with speech, they can nudge them earlier or later. If they are hard to read, they can make them larger.

**Why this priority**: Without subtitle and audio track support, a large share of video content (foreign-language films, multi-language releases, accessibility-dependent viewers) cannot be watched properly in GAV at all. Every other mainstream player offers it, so it is the biggest barrier to GAV being someone's default player.

**Independent Test**: Open a video that has two embedded audio tracks and one embedded subtitle track, plus a separate subtitle file with the same base name in the same folder. Verify the subtitles appear, the user can switch between all subtitle sources and "off", switch audio tracks, shift subtitle timing and change subtitle size.

**Acceptance Scenarios**:

1. **Given** a video with a subtitle file of the same base name in the same folder, **When** the user opens the video, **Then** that subtitle file is loaded and its text is displayed in sync with playback.
2. **Given** a video with embedded subtitle tracks, **When** the user opens the subtitle menu, **Then** every embedded track is listed by its language and/or title, alongside any external subtitle files and an "Off" option.
3. **Given** a playing video, **When** the user chooses "Load subtitle file…" and picks a supported subtitle file, **Then** it becomes the active subtitle track immediately without restarting playback.
4. **Given** a video with more than one audio track, **When** the user selects a different audio track, **Then** audio switches to that track within one second and playback position is preserved.
5. **Given** subtitles that appear 2 seconds late, **When** the user applies a subtitle delay adjustment, **Then** subtitles shift by the chosen amount and the current offset is shown briefly on screen.
6. **Given** subtitles are displayed, **When** the user increases or decreases subtitle size, **Then** text resizes immediately and the chosen size is kept for subsequent videos.
7. **Given** a video with no subtitle or alternate audio tracks, **When** the user views the controls, **Then** the subtitle and audio track options indicate there is nothing to choose, rather than disappearing silently or erroring.

---

### User Story 2 - Playback continuity (Priority: P2)

A viewer watching a long video or a series of files closes GAV partway through. When they reopen the same file later, it picks up where they left off. They can quickly reopen recently played files, shuffle their playlist, and save the playlist to a file to reload another day.

**Why this priority**: Long-form content (films, lectures, podcasts, audiobooks) is routinely watched across multiple sessions. Losing one's place is a frequent frustration, and reopening files through a file dialog each time is slow. This builds on the existing playlist and settings and does not depend on Story 1.

**Independent Test**: Play a 30-minute file to the 12-minute mark, close GAV, reopen the file and confirm GAV asks whether to resume from about 12:00 or start over, and that both choices work. Check that the file appears in the recent files list, that shuffle changes play order, and that a saved playlist reloads with the same items in the same order.

**Acceptance Scenarios**:

1. **Given** the user stopped a file partway through (beyond the first 5% and before the last 5% of its duration), **When** they open the same file again, **Then** GAV asks "Resume from <time>?" with Resume and Start over options, and does not start playing until the user chooses. Resume continues from the saved position. Start over plays from the beginning.
2. **Given** the user watched a file to within its last 5%, **When** they open it again, **Then** it starts from the beginning and the saved position is discarded.
3. **Given** files have been played, **When** the user opens the recent files list, **Then** the most recently played files appear newest first, and selecting one opens it.
4. **Given** a recent file has since been moved or deleted, **When** the user selects it, **Then** GAV shows a clear "file not found" message and offers to remove it from the list.
5. **Given** a playlist of several items, **When** the user turns shuffle on, **Then** the remaining items play in a random order without repeats until every item has played once.
6. **Given** a playlist, **When** the user saves it to a file and later opens that file in GAV, **Then** the playlist is restored with the same items in the same order, skipping (and reporting) any items that no longer exist.
7. **Given** "Restore last playlist" is turned on in settings and GAV was closed with items in the playlist, **When** GAV starts, **Then** the playlist is restored with the same items and order, and the item that was current is selected but not playing.
8. **Given** "Restore last playlist" is off (the default), **When** GAV starts, **Then** the playlist is empty.
9. **Given** the user wants privacy, **When** they choose "Clear history" in settings, **Then** saved positions and recent files are removed.

---

### User Story 3 - Precise navigation (Priority: P3)

A viewer reviewing footage wants to find an exact moment: step forward or back one frame at a time while paused, jump straight to a typed timestamp, or skip between chapters in a film or lecture. Power users want to drive all of this, along with mute, full-screen, speed and volume, from the keyboard.

**Why this priority**: GAV's existing frame capture, A-B repeat and zoom already appeal to people who study footage closely. Frame-accurate navigation and chapters complete that workflow. More keyboard shortcuts benefit every user. It is lower priority than Stories 1 and 2 because casual viewing works without it.

**Independent Test**: Open a video that contains chapters. Pause it and step forward and back by single frames. Type a timestamp into "Go to time". Jump to the next and previous chapter from both the controls and the keyboard. Confirm every listed shortcut works and is shown in the shortcuts reference.

**Acceptance Scenarios**:

1. **Given** a paused video, **When** the user presses the frame-forward key, **Then** exactly one frame advances and the displayed time updates.
2. **Given** a paused video not at the start, **When** the user presses the frame-back key, **Then** exactly one frame earlier is displayed.
3. **Given** a video with chapters, **When** it is loaded, **Then** chapter boundaries are marked on the seek bar and hovering a marker shows the chapter title.
4. **Given** a video with chapters, **When** the user chooses next or previous chapter, **Then** playback jumps to the start of the next chapter, or to the start of the current chapter (or the previous one if within the first 3 seconds of the current chapter).
5. **Given** any loaded media, **When** the user opens "Go to time" and enters a valid time (e.g. `1:23:45`, `83:45`, or `45`), **Then** playback jumps to that position. An out-of-range or malformed entry is rejected with an inline message.
6. **Given** media is loaded, **When** the user presses any shortcut listed in the shortcuts reference, **Then** the corresponding action happens, and the reference in settings lists every available shortcut.

---

### User Story 4 - System integration and network streams (Priority: P4)

A user listening to music or a podcast in GAV while working in another application presses the play/pause key on their keyboard or headset, or uses the operating system's now-playing controls, and GAV responds. The OS shows what is playing. The user can also paste a stream URL to play online media directly, without downloading it first.

**Why this priority**: This makes GAV feel like a proper citizen of the desktop, particularly for audio use where the window is often in the background. It is lowest priority because it adds convenience rather than unlocking content that is otherwise unplayable.

**Independent Test**: With GAV playing audio in the background, press the hardware play/pause, next and previous media keys and confirm GAV responds. Open the OS now-playing panel and confirm title (and artwork, when available) is shown and its controls work. Open a public HTTP(S) stream URL and confirm it plays.

**Acceptance Scenarios**:

1. **Given** GAV is playing and not focused, **When** the user presses the hardware play/pause media key, **Then** playback toggles.
2. **Given** a playlist with multiple items, **When** the user presses the next or previous media key, **Then** GAV moves to the next or previous item.
3. **Given** media is playing, **When** the user opens the operating system's now-playing panel, **Then** it shows the current title (and artist/album and artwork when the file has them), and its play/pause/next/previous controls operate GAV.
4. **Given** the user chooses "Open URL…" and enters a reachable HTTP(S) media or stream address, **When** they confirm, **Then** playback begins and the item appears in the playlist under its URL or stream title.
5. **Given** an unreachable or unsupported URL, **When** the user tries to open it, **Then** GAV shows a clear error message and stays responsive.

---

### Edge Cases

- A subtitle file is in an unsupported format or has an invalid encoding. GAV reports that it couldn't read it and keeps the previous subtitle choice.
- A subtitle file uses a non-UTF-8 text encoding. Common encodings are detected so text is not garbled.
- Several subtitle files match a video (e.g. `movie.en.srt`, `movie.fr.srt`). All are listed. The one matching the user's preferred subtitle language (if set) is chosen, otherwise the first alphabetically.
- An audio track switch is requested while the file is still loading. The switch applies once loading completes.
- A file's contents changed (re-encoded) since its position was saved, and the saved position exceeds the new duration. GAV starts from the beginning.
- The same file is reached by two different paths (e.g. a symlink). These are treated as separate entries. This is acceptable.
- Shuffle is turned on mid-playlist. The current item keeps playing and only upcoming items are reordered.
- A saved playlist contains relative paths. These resolve against the playlist file's own folder.
- Frame-back is used at the very first frame. Nothing happens and no error is shown.
- Frame stepping is attempted during playback. Playback pauses first, then steps.
- Chapter navigation is used on media without chapters. The controls are disabled or hidden and the shortcuts do nothing.
- A media key is pressed while no media is loaded. It is ignored.
- Another media application also listens for media keys. The OS's normal arbitration decides which application receives them, and GAV does not try to override it.
- A network stream drops mid-playback. GAV shows a connection error and does not freeze. Live streams with no fixed duration show live status instead of a seek bar position.
- A SubStation Alpha file references a font that is neither embedded nor installed. A fallback font is used and the rest of the styling still applies.
- A playlist auto-advances to a file with a remembered position. The resume prompt still appears and the playlist waits for the user's choice.
- GAV is launched with a file (command line, "Open with" or a second-instance hand-off) while "Restore last playlist" is on. The restored playlist loads first, then the new file is appended and played.
- The mini player is active. Subtitles, shortcuts, media keys and resume all keep working.

## Requirements *(mandatory)*

### Functional Requirements

**Subtitles and tracks (Story 1)**

- **FR-001**: The system MUST display subtitles from external subtitle files in SubRip (`.srt`), SubStation Alpha (`.ass`/`.ssa`) and WebVTT (`.vtt`) formats.
- **FR-001a**: The system MUST render SubStation Alpha subtitles with full styling fidelity: authored fonts, sizes, colours, outlines, shadows, exact positioning, rotation, karaoke timing and animated effects.
- **FR-001b**: The system MUST use fonts embedded in the media file for its subtitles when present, falling back to system fonts otherwise.
- **FR-002**: The system MUST display subtitle tracks embedded in the video file, where the file contains text-based subtitle tracks.
- **FR-003**: The system MUST automatically load an external subtitle file that shares the video's base name (optionally with a language suffix such as `.en`) from the same folder.
- **FR-004**: Users MUST be able to load an arbitrary subtitle file during playback, by menu or by dragging it onto the player.
- **FR-005**: Users MUST be able to select any available subtitle track, or turn subtitles off.
- **FR-006**: Users MUST be able to select any available audio track. Playback position is preserved across the switch.
- **FR-007**: Track menus MUST label tracks by language and title when the file provides them, and by track number otherwise.
- **FR-008**: Users MUST be able to shift subtitle timing earlier or later in 100 ms steps. The current offset is shown on screen and resets when a different file is opened.
- **FR-009**: Users MUST be able to change subtitle text size. The choice persists across sessions.
- **FR-010**: Users SHOULD be able to set a preferred subtitle language and preferred audio language, which are selected automatically when a file offers them.

**Playback continuity (Story 2)**

- **FR-011**: The system MUST remember the playback position of a file when playback stops, a different file is opened, or the application closes, provided the position is beyond the first 5% and before the last 5% of the file's duration.
- **FR-012**: On reopening a file with a remembered position, the system MUST ask the user whether to resume from that position or start over, and MUST NOT begin playback until the user chooses. The prompt MUST be answerable from the keyboard (Enter resumes, Esc starts over).
- **FR-013**: The system MUST retain remembered positions for at least the 200 most recently played files, discarding the oldest beyond that.
- **FR-014**: Resuming (saved positions) and recent files MUST each have their own setting, both off by default, so nothing is recorded until the user opts in. While either is off, nothing new is recorded for it. Existing history is kept until the user clears it (FR-016).
- **FR-015**: The system MUST maintain a list of the 10 most recently opened files, accessible from the main interface, newest first.
- **FR-015a**: Saved positions MUST NOT store file paths or names. They are keyed by a SHA-256 hash of the normalised full path, so a file is matched by hashing its path when it is opened. Full paths are stored only in the recent files list.
- **FR-016**: Users MUST be able to clear the recent files list, all remembered positions and the stored last-session playlist from settings.
- **FR-017**: Users MUST be able to toggle shuffle for the playlist. With shuffle on, each item plays exactly once before any item repeats.
- **FR-018**: Users MUST be able to save the current playlist to a file in the widely supported M3U playlist format, and to open such a file to restore the playlist.
- **FR-019**: When restoring a playlist, the system MUST skip missing entries and tell the user how many were skipped.
- **FR-019a**: The system MUST offer a "Restore last playlist" setting, off by default. When it is on, the playlist and current item at exit MUST be restored on next launch, without starting playback.

**Precise navigation (Story 3)**

- **FR-020**: Users MUST be able to step forward one frame and back one frame while video is paused.
- **FR-021**: The system MUST show chapter markers on the seek bar for media containing chapters, with the chapter title shown on hover.
- **FR-022**: Users MUST be able to jump to the next and previous chapter.
- **FR-023**: Users MUST be able to jump to a typed timestamp in `h:mm:ss`, `m:ss` or plain-seconds form. Invalid or out-of-range input is rejected with an inline message.
- **FR-024**: The system MUST provide the following keyboard shortcuts, following VLC's default layout. GAV's existing bindings (Space, Left/Right 5-second seek, scroll for volume, Ctrl+scroll for zoom, `I` for stats, double-click for full-screen) stay unchanged.

  | Action | Key |
  |--------|-----|
  | Volume up / down | Ctrl+Up / Ctrl+Down |
  | Mute | M |
  | Full-screen toggle / exit | F / Esc |
  | Speed slower / faster / normal | `[` / `]` / `=` |
  | Next frame / previous frame | E / Shift+E |
  | Next / previous chapter | Shift+N / Shift+P |
  | Next / previous playlist item | N / P |
  | Go to time | Ctrl+T |
  | Cycle subtitle track / audio track | V / B |
  | Subtitle delay earlier / later | G / H |
  | Open file / open URL | Ctrl+O / Ctrl+N |

  VLC has no default previous-frame key, so Shift+E is a GAV addition.
- **FR-025**: The shortcuts reference in settings MUST list every available shortcut and match actual behaviour.
- **FR-026**: Shortcuts MUST NOT trigger while the user is typing in a text field (e.g. playlist filter, go-to-time box).

**System integration and streams (Story 4)**

- **FR-027**: The system MUST respond to hardware or OS media keys for play/pause, stop, next and previous, including when the GAV window is not focused, on each supported desktop platform where the OS provides this capability.
- **FR-028**: The system MUST publish the current media's title, plus artist, album, duration and artwork when available, to the operating system's now-playing interface, and accept play/pause/next/previous/seek commands from it.
- **FR-029**: Users MUST be able to open a media stream by entering an HTTP or HTTPS URL.
- **FR-030**: The system MUST show a clear error when a URL cannot be opened or a stream is interrupted, without freezing the interface.
- **FR-031**: Live streams without a known duration MUST be indicated as live, and seeking MUST be disabled for them.
- **FR-031a**: When playback stalls waiting for data, the system MUST show a buffering indicator within about one second and remove it as soon as playback continues. The video and controls stay usable while it is shown.

**Cross-cutting**

- **FR-032**: All new user-facing settings MUST persist across application restarts, alongside the existing settings.
- **FR-033**: All new features MUST work in both the main window and the mini player where the relevant controls are applicable.

### Key Entities

- **Subtitle track**: A source of timed text for the current video. It is either embedded in the file or an external file, and has a language, a title, a display name and whether it is currently active.
- **Audio track**: An alternate audio stream in the current file, with a language, a title and whether it is currently active.
- **Playback history entry**: A remembered position for one file, made up of the file's location, the last position, the file's duration and when it was last played. It is used for resume and for the recent files list.
- **Saved playlist**: An ordered list of media locations (local paths or URLs). It is either a user-saved playlist file or the automatically kept last-session playlist (with its current item) used by "Restore last playlist".
- **Chapter**: A named section of a media file with a start time and a title.
- **Shortcut**: A key combination mapped to a player action, listed in the shortcuts reference.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For a video with a same-named subtitle file alongside it, subtitles are visible within 2 seconds of playback starting, with no user action.
- **SC-002**: Switching subtitle or audio track takes effect within 1 second and does not move the playback position by more than 1 second.
- **SC-003**: Subtitle timing can be corrected for a 2-second offset in 20 or fewer key presses.
- **SC-004**: After closing and reopening GAV, a partially watched file resumes within 2 seconds of its last position in 100% of cases where the file is unchanged.
- **SC-005**: A recently played file can be reopened in 2 clicks or fewer from the main window.
- **SC-006**: A saved playlist of 100 items reloads with identical order and contents.
- **SC-007**: Frame stepping advances or rewinds exactly one frame per key press in 95% or more of presses on common constant-frame-rate video.
- **SC-008**: Every action listed in FR-024 can be performed without the mouse.
- **SC-009**: Media key presses are acted on within 500 ms while GAV is in the background on each supported platform.
- **SC-010**: An unreachable stream URL produces an error message within 15 seconds, and the interface remains responsive throughout.
- **SC-011**: Styled SubStation Alpha sample files (signs, karaoke, positioned and rotated text) look the same in GAV as in a reference libass-based player (mpv) when paused frames at the fixture's defined sample timestamps (00:05, 00:12, 00:20) are compared side by side: same text positions, rotation, colours and karaoke highlight state.

## Assumptions

- Target users are individuals playing local media files on desktop Linux, Windows and macOS, as today. Mobile platforms are out of scope.
- Image-based embedded subtitles (e.g. DVD/Blu-ray bitmap subtitles) are out of scope for the first iteration. Only text-based tracks are required.
- Subtitle downloading from online services is out of scope.
- Playback history and recent files are stored locally on the user's machine only. No syncing across devices.
- Resume applies to local files only, not to network streams.
- Keyboard shortcut customisation (rebinding) is out of scope. A fixed, documented set is sufficient for this roadmap.
- Network stream support covers direct HTTP(S) media files and common adaptive streaming playlists. Authenticated streams, casting to other devices and DLNA/UPnP browsing are out of scope.
- OS now-playing and media-key integration depend on what each platform exposes. Where a platform offers no such capability, the feature may be absent on that platform without blocking the rest.
- Existing behaviours are kept: single-instance hand-off, the mini player, repeat modes, playlist filter and drag-and-drop.
- All four user stories are covered by a single plan. Each story still remains independently testable.
