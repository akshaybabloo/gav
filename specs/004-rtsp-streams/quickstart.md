# Quickstart: Validating RTSP Streams

How to check that the feature works. Behaviour is specified in
[contracts/stream-addresses.md](./contracts/stream-addresses.md) and
[contracts/rtsp-connection.md](./contracts/rtsp-connection.md).

## Prerequisites

- A checkout that builds with `just build`, and `just test` passing.
- `ffmpeg` on `PATH`, to publish a test picture and tone.
- [MediaMTX](https://github.com/bluenviron/mediamtx/releases) 1.21.2, as the RTSP server. Either
  the release binary, or the container image `bluenviron/mediamtx:1.21.2` run with host
  networking.
- `python3`, for the server that sends no media.
- Sound muted or the output device set to none while running the headless checks.

The files under `tests/rtsp/` are part of this feature:

| File | Purpose |
|---|---|
| `tests/rtsp/mediamtx.yml` | Server configuration: RTSP on `127.0.0.1:8554`, secure RTSP on `127.0.0.1:8322`, an open path `test`, and a path `cam` that needs user `viewer`, password `s3cr3tpw` |
| `tests/rtsp/silent_server.py` | Answers the RTSP handshake on `127.0.0.1:8555` and never sends media |
| `tests/rtsp/README.md` | The commands below, and how to make the certificate for secure RTSP |

## Start the test source

```text
mediamtx tests/rtsp/mediamtx.yml

ffmpeg -re -f lavfi -i testsrc=size=640x360:rate=25 -f lavfi -i sine=frequency=440 \
  -c:v libx264 -preset ultrafast -tune zerolatency -g 25 -pix_fmt yuv420p -c:a aac \
  -f rtsp -rtsp_transport tcp rtsp://127.0.0.1:8554/test
```

Publish to `rtsp://127.0.0.1:8554/cam` in the same way for the sign-in checks. To make the
server accept one transport only, set `rtspTransports` in the configuration to `[tcp]` or
`[udp]` and restart it.

Addresses used below:

| Name | Address |
|---|---|
| open | `rtsp://127.0.0.1:8554/test` |
| secure | `rtsps://127.0.0.1:8322/test` |
| camera | `rtsp://viewer:s3cr3tpw@127.0.0.1:8554/cam` |
| camera, wrong password | `rtsp://viewer:wrong@127.0.0.1:8554/cam` |
| camera, no credentials | `rtsp://127.0.0.1:8554/cam` |
| silent | `rtsp://127.0.0.1:8555/s` |
| nothing there | `rtsp://127.0.0.1:8599/test` |

## Automated checks

| Command | Expected |
|---|---|
| `just test` | Every test passes, including the new ones for addresses, the log filter, the connection logic, the failure mapping, playlists, history and the FFmpeg capability check |
| `LC_ALL=C LANG=C just test` | The same under the `C` locale, which is what CI uses |
| `just format-check` and `just lint` | No findings |

## Story 1: watch an RTSP stream

| # | Steps | Expected |
|---|-------|----------|
| Q1.1 | Open URL, enter *open*, confirm | Picture and sound within 5 seconds; the entry is in the playlist as a stream (SC-001) |
| Q1.2 | Look at the controls and use them | LIVE badge; no seek position; go to time, frame step, A-B repeat and seek preview unavailable; no quality menu and no subtitle tracks are offered; pause, volume, mute, full screen, mini player, brightness and contrast, zoom, the audio track menu and the keyboard's media keys work |
| Q1.3 | Close GAV, run `just run rtsp://127.0.0.1:8554/test` | It opens and plays the stream |
| Q1.4 | With GAV running, run the same command again | The running window opens the stream; no second window |
| Q1.5 | Turn on "remember positions", play *open* for a minute, close it, open it again | No resume prompt; `history.json` has no position for it (FR-006) |
| Q1.6 | Open URL, enter *nothing there* | "Could not open the stream…" within 15 seconds; the window responds throughout (SC-002) |
| Q1.7 | Open URL, enter `rtmp://127.0.0.1/x` | The hint under the field lists http, https, rtsp and rtsps; nothing is opened |
| Q1.8 | Run `just run rtmp://127.0.0.1/x` | The "Unsupported address" line on standard error names the four kinds |
| Q1.9 | Open *secure* | Plays in the same way |
| Q1.10 | Play *open*; stop the `ffmpeg` publisher for 5 seconds, then start it again | "Reconnecting…" appears; the stream is playing again within 10 seconds of the publisher returning; the playlist did not advance (SC-009) |
| Q1.11 | Play *open*; stop the publisher and leave it stopped | Three attempts, then "The stream was lost." within 45 seconds; the player is stopped |
| Q1.12 | Play *open*; stop the publisher; while "Reconnecting…" shows, open a local file | The file plays at once; no further attempt is made and no message appears later |
| Q1.13 | Play *open* with another entry after it in the playlist; stop the publisher | The next entry does not start while reconnecting or after the stream is lost |
| Q1.14 | Play *open*, pause for 30 seconds, play | It resumes, or reconnects if the server closed the session; no error is left on screen |
| Q1.15 | Play *open* for 30 minutes | No stop, freeze or loss of sound (SC-006) |
| Q1.16 | Publish to `test` with `-vn` (no picture) and play *open* | Sound plays; shown as live; no failure message |
| Q1.17 | Publish to `test` with `-an` (no sound) and play *open* | Picture plays; shown as live |
| Q1.18 | Play *open* in the mini player; stop the publisher for 5 seconds and start it again. Repeat with the main window minimised | The stream comes back in both cases |

## Story 2: choose TCP or UDP

Open the statistics overlay to read the transport in use.

| # | Steps | Expected |
|---|-------|----------|
| Q2.1 | Fresh settings; open Settings → Playback | "RTSP transport" is Automatic; TCP and UDP are the other choices |
| Q2.2 | Automatic; server accepts both; play *open* | Transport: UDP |
| Q2.3 | Automatic; server set to `[tcp]`; play *open* | Plays; Transport: TCP |
| Q2.4 | Automatic; play *silent* | After 5 seconds GAV moves to TCP (the script's output shows a second `SETUP` with TCP); "Could not open the stream…" within 15 seconds |
| Q2.5 | After Q2.3, close the stream and play *open* again | It starts over TCP straight away: about as fast as with TCP forced (FR-012a, SC-001) |
| Q2.6 | Restart GAV; server back to both; play *open* | Transport: UDP again; the memory did not survive the restart |
| Q2.7 | Set TCP; play *open* | Transport: TCP |
| Q2.8 | Set UDP; play *open* | Transport: UDP, with no restart of GAV (SC-004) |
| Q2.9 | Set UDP; server set to `[tcp]`; play *open* | "Could not open the stream over UDP. Choose Automatic or TCP in Settings."; the server log shows no TCP session |
| Q2.10 | Set UDP; play *silent* | The buffering indicator shows the whole time and the stream is never shown as playing; the same message within about 10 seconds |
| Q2.11 | While *open* plays over TCP, change the setting to UDP | The stream carries on over TCP; the next one opened uses UDP |
| Q2.12 | Set UDP; play *secure* | Plays; Transport: TCP (secure stream) |

## Story 3: credentials stay private

Start GAV with `just run --verbose` and keep its output in a file. Turn on recent files and
session restore.

| # | Steps | Expected |
|---|-------|----------|
| Q3.1 | Play *camera* | Plays |
| Q3.2 | Look at the window title, the playlist row, the statistics overlay (and paste what its copy button copies), and the operating system's now-playing display | The address appears as `rtsp://127.0.0.1:8554/cam` everywhere; `viewer` and `s3cr3tpw` appear nowhere |
| Q3.3 | Play another entry, then play *camera* again from the playlist | It plays without the credentials being entered again |
| Q3.4 | Close GAV. Search `history.json`, `session.m3u8` and the saved output for `s3cr3tpw` | Not found in any of them (SC-003) |
| Q3.5 | Start GAV; the session is restored; play the camera entry | "This stream needs a user name and password…"; its action opens the Open URL dialog with `rtsp://127.0.0.1:8554/cam` filled in |
| Q3.6 | Open the same address from the recent files menu | The same |
| Q3.7 | Play *camera, wrong password* with the setting on Automatic | "Signing in to the stream failed…" after the first attempt; the server log shows no TCP attempt following the UDP one |
| Q3.8 | Play *camera, no credentials* | "This stream needs a user name and password…" |
| Q3.9 | Play `https://user:pw@127.0.0.1:9/x.mp4` (it will fail to open) with verbose output | The message and the log show the address without `user:pw` (FR-022) |
| Q3.10 | Close GAV. Run `just run --verbose rtsp://viewer:s3cr3tpw@127.0.0.1:8554/cam` and keep the output | The stream plays. The output has no `s3cr3tpw`, including in the "Loading source" line that GAV writes itself (FR-018) |

## Story 4: RTSP entries in playlists

Make a playlist file with an `http` entry, the *open* and *secure* addresses with titles and a
`group-title`, the *camera* address, and an `rtmp://` entry.

| # | Steps | Expected |
|---|-------|----------|
| Q4.1 | Load the file | The RTSP entries are listed as streams with their titles and group; the `rtmp://` entry is left out and counted as unsupported (SC-005) |
| Q4.2 | Play each RTSP entry; use next and previous across them | Each plays with the transport from Settings |
| Q4.3 | Search for part of the camera's host; look at its row | Found; the row shows the address without credentials |
| Q4.4 | Serve the same file over HTTP and open its address | The same entries are kept |
| Q4.5 | Save the playlist | A dialog asks about passwords |
| Q4.6 | Choose "Save without passwords"; open the file in a text editor | No credentials in it |
| Q4.7 | Save again, choose "Save with passwords" | A second dialog says the passwords will be written as plain text; Cancel is its default button; nothing has been written yet |
| Q4.8 | Confirm the second dialog | The camera's address is written in full |
| Q4.9 | Save again, choose "Save with passwords", then Cancel in the second dialog | No file is written or changed |
| Q4.10 | Save again, choose Cancel in the first dialog | No file is written or changed |
| Q4.11 | Remove the camera entry and save | Saved with no question |

## Packages (SC-007)

On each of Linux, Windows and macOS, with the installed package rather than a development
build:

| # | Steps | Expected |
|---|-------|----------|
| QP.1 | Play *open* with TCP forced, then UDP forced, then Automatic | Plays each time, with the expected transport |
| QP.2 | Play *secure* | Plays |
| QP.3 | Play *camera* | Plays |

Windows and macOS need a person at that machine. Say so in the pull request if they were not
run.

## Nothing else changed (SC-008)

| # | Steps | Expected |
|---|-------|----------|
| QR.1 | Play a local video and a local audio file | As before, including resume |
| QR.2 | Open an HTTP file, an HLS stream with several qualities, and a remote playlist | As before, including the quality menu and stream subtitles |
| QR.3 | Open an HTTP address that does not answer | The existing message, after the existing time |
