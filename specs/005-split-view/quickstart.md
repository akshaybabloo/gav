# Quickstart: Validating the Split View

How to check that the feature works. Behaviour is specified in
[contracts/split-view.md](./contracts/split-view.md).

## Prerequisites

- A checkout that builds with `just build`, and `just test` passing.
- `ffmpeg` on `PATH`, to make test clips and publish test cameras.
- Four local 1080p 30 fps videos with sound, each at least 90 seconds, and one audio-only file.
  To make them:

  ```text
  for i in 1 2 3 4; do
    ffmpeg -f lavfi -i testsrc2=size=1920x1080:rate=30 -f lavfi -i sine=frequency=$((300+i*100)) \
      -t 90 -c:v libx264 -preset veryfast -pix_fmt yuv420p -c:a aac clip$i.mp4
  done
  ```

- For the camera checks: MediaMTX 1.21.2 with `tests/rtsp/mediamtx.yml`, which gains the open
  paths `wall1` to `wall4`, and an `ffmpeg` publisher for each (see `tests/rtsp/README.md`).
- Sound muted or the output device set to none while running headless checks.
- A playlist with the four clips, the audio-only file, an HTTP video, and `wall1`–`wall4`.

## Automated checks

| Command | Expected |
|---|---|
| `just test` | Every test passes, including the new ones for `TileGrid`, the RTSP open gate, the shared TCP memory, the playlist position role, next and previous skipping entries, and per-player preview names |
| `LC_ALL=C LANG=C just test` | The same under the `C` locale |
| `just lint` | No findings on lines this feature adds |

## Story 1: watch up to four videos

| # | Steps | Expected |
|---|-------|----------|
| Q1.1 | Nothing playing. Select the four clips, right-click, "Play in split view" | All four play in a 2×2 grid in playlist order within 5 s; only clip 1 (top left) is heard; it has the selection border (SC-001) |
| Q1.2 | Same with two clips, then with three | Two side by side; three in a 2×2 grid with the bottom right empty |
| Q1.3 | Select five entries and right-click | "Play in split view" is unavailable and its tooltip says a split view holds up to four videos |
| Q1.4 | Nothing playing; right-click a single entry | "Play in split view" is unavailable |
| Q1.5 | Play clip 1 alone; at 20 s, right-click clip 2, "Play in split view" | Clip 1 carries on from 20 s without restarting, on the left, still heard; clip 2 plays on the right, muted |
| Q1.6 | In a split view of four, press Stop three times | Each Stop closes the selected tile and the rest move up; with one left, it carries on in the single view with sound |
| Q1.7 | In a split view, View > "End split view" | The selected video carries on alone; the others stop |
| Q1.8 | Play two clips in a split view for 10 minutes, then four for 10 minutes, with Stats for nerds on for the selected tile and checked on each | Each plays at 30 fps with less than 1% of frames dropped; the window answers clicks and keys at once (SC-003). Note whether hardware decoding was in use |

## Story 2: selection and controls

| # | Steps | Expected |
|---|-------|----------|
| Q2.1 | In a split view of four, click each tile in turn | The sound and the border move to the clicked tile at once; never two tiles heard (SC-002) |
| Q2.2 | Press 1, 2, 3, 4 | The tile at that position is selected |
| Q2.3 | Select tile 2; pause, seek, change speed, change audio track, load a subtitle file, zoom, change brightness, set A-B repeat, take a screenshot | Only tile 2 changes; select another tile and back: tile 2 kept all of them, and the control bar shows them (SC-006) |
| Q2.4 | Set repeat "loop" on tile 3 only; wait for the clips to end | Tile 3 starts again; the others stop at their end and keep their place; nothing moves on to the next entry |
| Q2.5 | Look at the window title, seek bar, statistics overlay and the system's now-playing display while changing the selection | Each shows the selected tile |
| Q2.6 | Mute, select another tile, unmute | Nothing heard while muted; after unmuting only the selected tile is heard, at the same volume |
| Q2.7 | Open the playlist panel | It lies over the right side; the tiles do not move; the four entries show positions 1–4; the selected one is the current row |
| Q2.8 | Put the audio-only file in a tile | It shows what the single view shows for audio; it is heard only when selected |
| Q2.9 | Try the mini player button in a split view | Unavailable |

## Story 3: focus and full screen

| # | Steps | Expected |
|---|-------|----------|
| Q3.1 | Four clips in a split view; double-click tile 2 | Tile 2 fills the area and is heard; tiles 1, 3 and 4 are paused |
| Q3.2 | Double-click again | Full screen, tile 2 still filling it |
| Q3.3 | Double-click again | Leaves full screen; the grid is back, every tile in its place; 1, 3, 4 continue from where they paused, muted (SC-004) |
| Q3.4 | Focus tile 2, go full screen, press Esc; press Esc again | First Esc leaves full screen with tile 2 still filling the area; second Esc brings the grid back |
| Q3.5 | Focus tile 2, press 4, then 1 | Tile 4 fills the area, heard, continuing from where it paused; tile 2 paused; then tile 1 |
| Q3.6 | Pause tile 3 by hand; focus tile 1; come back | Tile 3 is still paused; tiles 2 and 4 continue |
| Q3.7 | `wall1`–`wall4` in a split view; focus `wall2` for 30 s; come back | `wall1`, `wall3` and `wall4` show the current time of the test picture (they kept playing), not a picture from 30 s ago |
| Q3.8 | Focus a tile and press Stop | The focused tile closes and the grid comes back with the rest |
| Q3.9 | One video alone; double-click | Full screen toggles as before |

## Story 4: changing the split view

| # | Steps | Expected |
|---|-------|----------|
| Q4.1 | Split view of two; select tile 2; File > Open the HTTP video | It replaces tile 2's video in the same place |
| Q4.2 | Double-click (Play) an entry in the playlist that is in tile 1 | Tile 1 becomes selected; nothing restarts |
| Q4.3 | Split view of two; drop a clip onto tile 1 | It replaces tile 1's video |
| Q4.4 | Split view of three; drop a clip onto the empty bottom-right cell | It becomes tile 4, muted; the selection does not change |
| Q4.5 | Split view of four; drop a file anywhere; use "Play in split view" on another entry | "A split view holds up to four videos"; nothing changes |
| Q4.6 | Select tile 1; press next, then previous | Tile 1 moves to the next playlist entry that is not in a tile, then back |
| Q4.7 | Remove tile 3's entry from the playlist | Tile 3 closes and the rest move up |
| Q4.8 | Close GAV in a split view and start it again with session restore on | The playlist is restored; no split view |

## Cameras and failures

| # | Steps | Expected |
|---|-------|----------|
| QC.1 | Publish to `wall1`–`wall4`; select the four entries; "Play in split view" | All four play within 10 s; the statistics overlay shows each tile's transport (SC-005) |
| QC.2 | With the server set to `[tcp]` and Automatic, play `wall1`–`wall4` | All four play over TCP |
| QC.3 | Put `rtsp://127.0.0.1:8599/x` (nothing there) and the `cam` address with a wrong password in a split view with two clips | Each camera tile shows its own failure; the messages start with the entry's title and give the right kind (not found or sign-in); the clips carry on (FR-025) |
| QC.4 | Stop the publisher of `wall2` while four cameras play | Only tile 2 shows "Reconnecting…", visible on tile 2 while tile 1 is selected, and comes back when the publisher returns |
| QC.5 | An entry with a file that cannot be played, in a split view | The error names that entry; the other tiles carry on |

## Nothing else changed (SC-007)

| # | Steps | Expected |
|---|-------|----------|
| QR.1 | Run the single-view checks of feature 002 (playlist) and 004 (RTSP) quickstarts that cover playing, next and previous, resume, the mini player and reconnecting | Same results as before |
