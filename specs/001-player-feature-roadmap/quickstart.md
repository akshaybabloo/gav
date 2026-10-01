# Quickstart: Validating the Player Feature Roadmap

How to prove each user story works end to end. Automated checks come first, then manual scenarios
for UI behaviour that can't reasonably be automated (Constitution Principle V). Contracts and data
rules are linked rather than repeated.

## Prerequisites

- A GAV build with the new vcpkg dependencies (`libass`, `uchardet`, `ffmpeg[iconv]`, plus
  `ffmpeg[openssl]` on Linux/macOS). Configure and build as in `CLAUDE.md`.
- Test media in `tests/data/` (added by the implementation):

| File | Purpose |
|------|---------|
| `multi.mkv` | 2 audio tracks (eng, jpn), 1 embedded ASS track with an attached font, 3 chapters, 25 fps constant frame rate |
| `multi.en.srt`, `multi.fr.srt` | Sidecar subtitles. The French one is Windows-1252 encoded with accented characters |
| `styled.ass` + `styled.mp4` | Positioned and rotated signs and a karaoke line, for checking fidelity |
| `long.mp4` | At least 30 minutes, for resume |
| `playlist-relative.m3u8` | Relative paths and one missing entry |

- A reference player that uses libass (mpv or VLC) for the SC-011 comparison.

## Automated checks

```bash
cd build && ctest --output-on-failure
```

These must pass on all CI platforms:

| Test file | Covers |
|-----------|--------|
| `test_playbackutils.cpp` | Time parsing (R13), chapter navigation (R12), resume window 5%/95% (FR-011) |
| `test_playlistio.cpp` | [playlist-format.md](./contracts/playlist-format.md): round trip of 100 entries (SC-006), relative paths, BOM/CRLF, skipped counts, `#GAV-CURRENT` |
| `test_shuffleorder.cpp` | Every item once per cycle, mid-playlist enable, removal remapping (FR-017) |
| `test_playbackhistory.cpp` | 200-entry eviction (FR-013), recent list of 10 without duplicates, corrupt-file recovery |
| `test_subtitlefiles.cpp` | Sidecar discovery and preferred-language choice (R5) |
| `test_probeprotocol.cpp` | Message parser, plus golden `.jsonl` output for `multi.mkv` and `multi.fr.srt` ([probe-protocol.md](./contracts/probe-protocol.md)) |
| `test_subtitlerenderer.cpp` | Renders `styled.ass` at fixed timestamps and checks for non-empty pixels in the expected regions (rotation and position) |

## Story 1: Subtitles and tracks

| # | Steps | Expected |
|---|-------|----------|
| Q1.1 | `./build/gav tests/data/multi.mkv` | English sidecar subtitles show within 2 s (SC-001) |
| Q1.2 | Open the subtitle menu | Lists the embedded "Signs (eng)", `multi.en.srt`, `multi.fr.srt` and Off (FR-007) |
| Q1.3 | Choose `multi.fr.srt` | Accented characters display correctly, so encoding detection worked |
| Q1.4 | Press `V` repeatedly | Cycles through every track and Off, showing the track name each time |
| Q1.5 | Press `B` | Audio switches to jpn within 1 s, and the position moves by less than 1 s (SC-002) |
| Q1.6 | Press `H` 20 times | The indicator shows `+2000 ms` and subtitles are 2 s later (SC-003). Opening another file resets it to 0 |
| Q1.7 | Change the subtitle size in the menu, then restart GAV | The size is kept (FR-009) |
| Q1.8 | `./build/gav tests/data/styled.mp4`, load `styled.ass`. Compare paused frames at 00:05, 00:12 and 00:20 with mpv | Positions, rotation, colours and karaoke highlighting match (SC-011) |
| Q1.9 | Play `multi.mkv` and check the embedded track's font | The attached font is used, not a fallback (FR-001b) |
| Q1.10 | Drag `multi.en.srt` onto the window while another file plays | It becomes the active track without restarting playback (FR-004) |
| Q1.11 | Linux AppImage only: run the packaged AppImage on a clean VM | Subtitles render using system fonts, so the fontconfig workaround works (research R2) |
| Q1.12 | Open the mini player while subtitles are on | Subtitles display in the mini player (FR-033) |
| Q1.13 | Make the probe crash (`kill -SEGV` its PID) | A snackbar appears, playback continues, and the embedded track falls back to plain text |

## Story 2: Playback continuity

| # | Steps | Expected |
|---|-------|----------|
| Q2.1 | Play `long.mp4` to 12:00, then quit | `history.json` in the app data folder has an entry around 720000 ms |
| Q2.2 | Reopen `long.mp4` | Playback is held and the prompt reads "Resume from 12:00?" Enter resumes within 2 s of 12:00 (SC-004) |
| Q2.3 | Reopen it and press Esc | Plays from 0:00 and the entry is removed |
| Q2.4 | Seek to the last 5% and quit, then reopen | No prompt appears (FR-011) |
| Q2.5 | Open the recent files menu | `long.mp4` is first and opens in 2 clicks or fewer (SC-005) |
| Q2.6 | Rename the file, then choose it from recent files | "File not found" appears with a Remove action |
| Q2.7 | Load 5 items, turn shuffle on, and let all of them play | Each item plays exactly once (FR-017) |
| Q2.8 | Save the playlist, clear it, and open the saved `.m3u8` | Same items in the same order (FR-018) |
| Q2.9 | Open `playlist-relative.m3u8` | Relative entries resolve, and the message says 1 item was skipped (FR-019) |
| Q2.10 | Turn "Restore last playlist" on, load 3 items, select the second, then quit and relaunch | The 3 items are restored with the second selected and not playing (FR-019a) |
| Q2.11 | With restore on, run `./build/gav tests/data/multi.mkv` | The restored items load and `multi.mkv` is appended and plays |
| Q2.12 | Settings → Clear history | Recent files, positions and the session playlist are all removed (FR-016) |

## Story 3: Precise navigation

| # | Steps | Expected |
|---|-------|----------|
| Q3.1 | Pause `multi.mkv` at 00:10 and press `E` 50 times while watching the stats overlay's frame time | The time goes up by 40 ms on each press in at least 48 of 50 presses (SC-007) |
| Q3.2 | Press `Shift+E` 10 times | Goes back one frame on each press. At 0:00 nothing happens |
| Q3.3 | Hover over the seek bar | 3 chapter markers show, and hovering one shows its title (FR-021) |
| Q3.4 | Press `Shift+N`, then `Shift+P` twice within 3 s | Next chapter, then the current chapter's start, then the previous chapter (R12) |
| Q3.5 | Press `Ctrl+T` and enter `1:05`, `65`, `1:75` and `9:99:99` | The first two jump to 1:05. The last two are rejected with an inline message (FR-023) |
| Q3.6 | Click into the playlist filter, then type `m`, `n` and `f` | Text is typed and no shortcuts fire (FR-026) |
| Q3.7 | Go through every row of [keyboard-and-cli.md](./contracts/keyboard-and-cli.md) without the mouse | Every action works, and the settings reference matches the table (SC-008, FR-025) |

## Story 4: System integration and streams

| # | Steps | Expected |
|---|-------|----------|
| Q4.1 | Linux: play audio and run `playerctl -p gav metadata` | Shows the title and length. `playerctl -p gav play-pause` toggles within 500 ms (SC-009) |
| Q4.2 | Windows: play audio and press the keyboard's media keys with another app focused | GAV responds. The volume flyout shows the title and artwork |
| Q4.3 | macOS: play audio and use Control Center's Now Playing | Title and controls work, and scrubbing seeks |
| Q4.4 | Use the media next/previous keys with a 3-item playlist | Moves between playlist items |
| Q4.5 | Press a media key with no media loaded | Nothing happens and nothing errors |
| Q4.6 | Press `Ctrl+N` and enter a public HTTPS MP4 URL | Plays, and the playlist shows the URL or title (FR-029) |
| Q4.7 | Open a public HLS live stream | Shows LIVE and seeking is disabled (FR-031) |
| Q4.8 | Open `https://10.255.255.1/x.mp4` (unreachable) | An error appears within 15 s and the UI stays responsive (SC-010) |
| Q4.9 | `./build/gav https://…/file.mp4` with GAV already running | The running instance plays the URL (single-instance hand-off) |
| Q4.10 | Repeat Q4.6 with the packaged CI build (custom FFmpeg plugin) on Linux and macOS | HTTPS plays, which confirms `ffmpeg[openssl]` reached the plugin (R11) |
