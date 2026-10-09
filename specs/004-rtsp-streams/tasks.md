---

description: "Task list for RTSP Streams over TCP or UDP"
---

# Tasks: RTSP Streams over TCP or UDP

**Input**: Design documents from `specs/004-rtsp-streams/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md)

**Tests**: Included. The constitution requires unit tests for logic that runs without a GUI, and
the plan puts the address rules and the connection decisions in two units that have no playback
code in them for that reason. What needs a server or a screen is checked with
[quickstart.md](./quickstart.md).

**Parent issue**: #179. Sub-issues are created under it before a step starts, and its branch is
named `<issue-number>-<slug>`.

**Organization**: Tasks are grouped by user story, in the spec's priority order. They are
delivered in the plan's five steps, which is a different order: credentials handling goes first.
The table under [Delivery order](#delivery-order) says which tasks make up each pull request;
follow that table, not the task numbers, when deciding what to do next.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on unfinished tasks)
- **[Story]**: The user story the task belongs to (US1–US4)

## Path Conventions

GAV keeps every source file at the repository root. QML files belong to the `gavqml` module, and
C++ units are registered in `qt_add_qml_module(appgav … SOURCES …)` in `CMakeLists.txt`. Tests
live in `tests/` and are compiled into the `gav_tests` target, which lists the units it needs
again. Build, test, format and lint only through `just`, which uses `build-cli/`. Run the tests
under `LC_ALL=C LANG=C` as well. Headless runs of the application are silent. Icons in QML stay
as `"\uXXXX"` escapes. Default to no code comments.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: The local RTSP source that the quickstart and the headless checks use

- [ ] T001 [P] Create `tests/rtsp/mediamtx.yml` for MediaMTX 1.21.2: RTSP on `127.0.0.1:8554` with `rtspTransports: [udp, tcp]`, RTP on `127.0.0.1:8000` and RTCP on `127.0.0.1:8001`; `rtspEncryption: "optional"` with secure RTSP on `127.0.0.1:8322` and `server.key` / `server.crt` next to the file; every other protocol, the API, metrics and playback off; `authMethod: internal` with a user `any` that may publish to every path and read path `test`, and a user `viewer` with password `s3cr3tpw` that may read path `cam`; paths `test` and `cam`.
- [ ] T002 [P] Create `tests/rtsp/silent_server.py` (standard library only): listens on `127.0.0.1:8555`, answers `OPTIONS`, `DESCRIBE` (an SDP with one H.264 video track), `SETUP` (echoing the requested transport, UDP or interleaved TCP), `PLAY` and anything else with `200 OK`, never sends media, and prints each `SETUP` with its transport and a time stamp so that a fallback to TCP can be seen.
- [ ] T003 [P] Create `tests/rtsp/README.md` with the commands of [quickstart.md → Start the test source](./quickstart.md#start-the-test-source): where to get MediaMTX 1.21.2 (release binary or the `bluenviron/mediamtx:1.21.2` image with host networking), the `ffmpeg` publisher command for `test` and `cam` and its variants without picture (`-vn`) and without sound (`-an`), the `openssl` command that makes `server.key` and `server.crt`, how to restrict the server to one transport, and how to start the silent server. Add `tests/rtsp/server.key` and `tests/rtsp/server.crt` to `.gitignore`. Nothing binary is committed.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Step**: 1, Addresses and credentials (with the first block of Phase 5) · **Issue**: #179 · **Branch**: `179-stream-addresses`

**Purpose**: One definition of "an address GAV plays" and of its display form, used everywhere,
and no credentials in the log

**⚠️ CRITICAL**: No story can start until this phase is complete. In this phase the stream
schemes stay `http` and `https`; nothing new is accepted yet.

- [ ] T004 Create `streamaddress.h` / `streamaddress.cpp` and add them to `appgav` and `gav_tests` in `CMakeLists.txt`. A namespace or static class `StreamAddress` with, per [data-model.md → Stream address](./data-model.md#stream-address):
  - `isStream(url)`: the scheme, compared without regard to case, is in the list of stream schemes (in this task `http` and `https`), and the host is not empty
  - `isRtsp(url)`: the scheme is `rtsp` or `rtsps`; `isSecureRtsp(url)`: the scheme is `rtsps`
  - `hasPassword(url)`: "a password is present and not empty"
  - `displayAddress(url)`: "the same address with user name and password removed. Everything else, including percent-encoding, is unchanged"; a local file or an address without credentials is returned unchanged
  - `sourceKey(url)`: "host in lower case, and the port (554 for `rtsp`, 322 for `rtsps` when absent)"
  - `withoutCredentials(text)`: replaces every `scheme://user:password@` and `scheme://user@` inside arbitrary text with `scheme://` ([contracts/stream-addresses.md → Logging](./contracts/stream-addresses.md#logging))
- [ ] T005 [P] Create `tests/test_streamaddress.cpp` and add it to `gav_tests`. Cover: the five rows of the display-address table in [contracts/stream-addresses.md](./contracts/stream-addresses.md#the-display-address); a user name without a password; a password with percent-encoded characters; an IPv6 host; an upper-case scheme; `hasPassword` for each of those; `sourceKey` with and without a port for both RTSP schemes; `isStream` true for `http` and `https` and false for `rtmp`, `file`, a relative path and an address without a host; `withoutCredentials` on text with no address, with one, with two, and on the FFmpeg line from [research R7](./research.md).
- [ ] T006 In `playbackutils.h` / `playbackutils.cpp`, expose the rules to QML as `Q_INVOKABLE` functions that call `StreamAddress`: `isStreamAddress(url)`, `isRtspAddress(url)`, `displayAddress(url)` and `hasPassword(url)`. Add one test per function to `tests/test_playbackutils.cpp`.
- [ ] T007 Replace the scheme checks that decide "this is a stream to play" with `StreamAddress::isStream` in `main.cpp` (`sourcePathToUrl`), `playlistio.cpp` (the two entry checks in the line loop only), `playlistmodel.cpp` (`makeEntry`, entry kind), `playbackhistory.cpp` (`isNetworkUrl`) and `mediasession.cpp` (`requestOpenUri`), and build the scheme list of `mediasession_mpris.cpp` (`supportedUriSchemes`) from the same list. Leave as they are, `http` and `https` only: the playlist download in `playlistio.cpp`, logos in `playlistio.cpp`, `playlistmodel.cpp` and `logoprovider.cpp`, and everything in `hlsmaster.cpp`, `streamquality.cpp` and `probemessage.cpp` ([research R8](./research.md)).
- [ ] T008 Replace the three QML scheme checks with `PlaybackUtils.isStreamAddress`: `isStreamUrl` in `Main.qml`, the check in `RecentFilesMenu.qml`, and the validation in `OpenUrlDialog.qml`.
- [ ] T009 Create `credentialfiltersink.h`: a `spdlog` sink that wraps another sink and passes the text of every line through `StreamAddress::withoutCredentials` before forwarding it. In `main.cpp`, build the application's logger (`initLogging`) and the probe subprocess's logger (`runProbe`) on it, so that the lines that come through Qt's message handler and the lines GAV writes to the logger directly (`Loading source`, `Parsed as URL`, the probe's argument list in `mediaprobe.cpp`, the subtitle source in `subtitlecontroller.cpp`) are all covered at one point. Use the same function on the address printed by the "Unsupported address" line of `sourcePathToUrl`, which does not go through the logger. Add tests to `tests/test_streamaddress.cpp`: a logger built on the filter over a string sink is given a plain line and a formatted line that each contain an address with credentials, and the output has the display address only ([research R7](./research.md), FR-018).
- [ ] T010 Check the phase: `just build`; `just test` and `LC_ALL=C LANG=C just test` pass; a headless, silent run opens a local file and an `http` stream as before; `gav rtsp://127.0.0.1/x` is still refused.

**Checkpoint**: Every place agrees on what a stream address is. No behaviour has changed except that log lines no longer contain credentials.

---

## Phase 3: User Story 1 - Watch an RTSP stream (Priority: P1) 🎯 MVP

**Goal**: An RTSP address plays as a live stream from every way in, fails with a clear message,
and comes back by itself after a drop.

**Independent Test**: With the test source running, open `rtsp://127.0.0.1:8554/test` from the
Open URL dialog and from the command line; picture and sound play and the stream is shown as
live (quickstart Q1.1–Q1.18).

### Play (Step 2; own issue and branch; after Step 1)

- [ ] T011 [P] [US1] Create `tests/test_rtspsupport.cpp` and add it to `gav_tests`: through the FFmpeg API, the `rtsp` input format exists (`av_find_input_format`), and the input protocols `tcp`, `udp`, `rtp` and `tls` are listed (`avio_enum_protocols`). This is what proves FR-028 for the vcpkg FFmpeg on every CI platform ([research R1](./research.md)).
- [ ] T012 [US1] In `streamaddress.cpp`, add `rtsp` and `rtsps` to the stream schemes, and update `tests/test_streamaddress.cpp` and `tests/test_playbackutils.cpp` (RTSP addresses are streams; `rtmp` is not). From here the command line, a second launch, the operating system's open request and playlist entries accept them through T007 and T008.
- [ ] T013 [US1] Create `rtspconnection.h` / `rtspconnection.cpp` and add them to `appgav` and `gav_tests`: the connection state machine of [data-model.md → Connection](./data-model.md#connection), with no Qt Multimedia in it. The caller passes events and the elapsed time in milliseconds; the class returns what to do next. In this task:
  - states Idle, Connecting, Playing, Failed
  - `open(address)` plans the attempts for Automatic: "UDP, then TCP" for `rtsp://`, TCP only for `rtsps://` ([contracts/rtsp-connection.md → Attempts](./contracts/rtsp-connection.md#attempts)); every attempt has a network timeout of 5000 ms
  - events: first picture, player error with a failure kind, time passing, close
  - "An attempt fails" exactly as the three rows of that contract: an error before success (all attempts); 5 seconds without success (the UDP attempt of Automatic); 12 seconds without success (the last attempt)
  - a Sign-in or Not found failure on any attempt ends in Failed with no further attempt
  - outputs: open with this transport and timeout; failed with this kind, the transport tried and whether it was forced; the transport in use (`TCP`, `UDP` or empty)
  - close from any state returns to Idle and cancels what would follow
- [ ] T014 [P] [US1] Create `tests/test_rtspconnection.cpp` and add it to `gav_tests`, one test per row of [research R4](./research.md): UDP attempt succeeds; UDP errors and TCP follows; UDP shows nothing for 5 s and TCP follows; TCP reports an error and the result is Failed with kind Other; the 12 s backstop; Sign-in on the UDP attempt gives Failed with no TCP attempt; Not found the same; `rtsps://` makes one TCP attempt; close during each state cancels.
- [ ] T015 [US1] In `rtspconnection.cpp`, add the mapping from FFmpeg's failure description to the kinds of [data-model.md → Open failure](./data-model.md#open-failure): text containing `401` or `403` is Sign-in, `404` or `400` is Not found, anything else or empty text is Other ([research R5](./research.md)). In `tests/test_rtspconnection.cpp`, build the texts with `av_strerror` for `AVERROR_HTTP_UNAUTHORIZED`, `AVERROR_HTTP_FORBIDDEN`, `AVERROR_HTTP_NOT_FOUND`, `AVERROR_HTTP_BAD_REQUEST` and `AVERROR(ECONNREFUSED)` and check each maps as stated.
- [ ] T016 [US1] In `main.cpp`'s `logOutput`, when a message of the category `qt.multimedia.ffmpeg.mediadataholder` contains "FFmpeg error description", keep its text as the most recent open-failure description behind a mutex, with a function to take and clear it, declared in `rtspconnection.h`. The handler is called from Qt's loading thread.
- [ ] T017 [US1] In `custommediaplayer.h` / `custommediaplayer.cpp`, drive `RtspConnection` when the source is an RTSP address ([contracts/rtsp-connection.md](./contracts/rtsp-connection.md)):
  - before each attempt, set `QT_FFMPEG_RTSP_TRANSPORT` to `tcp` or `udp` with `qputenv` and the playback options' network timeout to 5000 ms, then set the player's source; for every other source restore the existing 10000 ms ([research R2](./research.md))
  - do not start the 120-second stream load timer for RTSP
  - report "first picture" when the video sink delivers its first frame or the position first moves
  - on a player error before success, take the failure description (T016), map it (T015) and pass it on. No error text of the player is matched: any error before the first picture fails the attempt
  - report `buffering` as true from the open until the first picture, because the underlying player reports "loaded" and "playing" when no media has arrived ([research R4](./research.md)); the interface must show the stream as connecting, never as a playing black picture (FR-004)
  - report `live` as true for every RTSP source, whatever duration the source reports, so that the LIVE badge shows and the seek controls stay off (FR-005)
  - a single-shot timer for the next deadline the state machine names
  - on Failed, stop the player and emit a new signal `rtspFailed(kind, message)` with kind one of `signIn`, `signInNeeded`, `notFound`, `transport`, `other`, `lost`, and the message from [the contract's table](./contracts/rtsp-connection.md#messages); do not also emit `errorOccurred` for it
  - new read-only properties `transportInUse` and `displaySource`
  - closing, stopping or setting another source closes the connection first
- [ ] T018 [US1] In `main.cpp`, change the refused-address line to "Unsupported address (http, https, rtsp and rtsps streams can be opened): " followed by the display address ([contracts/stream-addresses.md → Texts](./contracts/stream-addresses.md#accepted-addresses)).
- [ ] T019 [US1] In `OpenUrlDialog.qml`, set the description and the hint to the texts of that table, and add a way to open the dialog with an address already in the field.
- [ ] T020 [US1] In `Main.qml`, handle `rtspFailed`: show the message in the snackbar; for `signIn` and `signInNeeded` give it the action "Open URL", which opens the Open URL dialog with the stream's display address filled in.
- [ ] T021 [P] [US1] In `tests/test_playbackhistory.cpp` and `tests/test_playlistmodel.cpp`: an RTSP address counts as a network address, so no resume position is stored for it (FR-006), and an RTSP entry has the kind `Stream`.
- [ ] T022 [US1] Check the step with the test source, headless and silent: quickstart Q1.1–Q1.9 (plays over the Automatic plan, LIVE badge and disabled seek controls, command line, second launch, no resume, unreachable address within 15 s, refused scheme, secure address), Q1.16–Q1.17 (a stream with no picture and one with no sound), and the built-in fallback of Q2.3–Q2.4. The statistics overlay row for the transport arrives in Step 3, so read the transport from the server's log here (`is reading from path 'test', with TCP`) and from the silent server's output. While a stream is connecting, the buffering indicator shows and the controls do not show it as playing. `just test` and `LC_ALL=C LANG=C just test` pass. Fix what fails.

**Checkpoint**: RTSP addresses play. Ships as its own PR, after Step 1.

### Reconnect (Step 4; own issue and branch; after Step 2)

- [ ] T023 [US1] In `rtspconnection.h` / `rtspconnection.cpp`, add reconnecting per [contracts/rtsp-connection.md → Reconnecting](./contracts/rtsp-connection.md#reconnecting): a `lost` event that is accepted only in Playing; an `ended` event that carries whether the source had reported a duration, and is a loss only when it had not (a source with a duration has reached its real end: go to Idle, no reconnecting); state Reconnecting with "Attempts: up to 3"; "Attempt *n* starts 2, 9 and 16 seconds after the loss, or when the previous attempt ends if that is later"; each attempt is a last attempt with the transport that was in use; a Sign-in failure stops reconnecting with that kind; three failures give Failed with kind Lost; `lost` while paused goes to Idle and remembers that play must open the stream again.
- [ ] T024 [P] [US1] In `tests/test_rtspconnection.cpp`, add one test per row of the reconnecting table: first attempt succeeds; the second; the third; all three fail; attempt start times with fast and with slow failures; Sign-in during an attempt; close during reconnecting; `lost` ignored in Connecting; `lost` while paused; `ended` without a duration reconnects; `ended` with a duration goes to Idle with no attempt.
- [ ] T025 [US1] In `custommediaplayer.h` / `custommediaplayer.cpp`: for an RTSP stream that is Playing and not paused, report `ended`, with whether the source had a duration, when the player reports the end of the media, and `lost` on an error or when the position has not moved for 5 seconds ([research R6](./research.md)); add the read-only property `reconnecting`; while reconnecting keep `live` and `mediaLoaded` true, report `buffering` as true, and do not pass on the end of the media; when a source with a duration ends, pass the end of the media on as for any other entry; when a stream was lost while paused, `play()` opens it again.
- [ ] T026 [US1] In `MediaComponent.qml`, show the text "Reconnecting…" with the buffering indicator while `reconnecting` is true. In `Main.qml`, show "The stream was lost." for the kind `lost`. Confirm in `MediaControlsComponent.qml` that the end-of-media handler does not advance the playlist for an RTSP stream that is reconnecting or was lost.
- [ ] T027 [US1] Check the step with the test source, stopping and restarting the `ffmpeg` publisher: quickstart Q1.10–Q1.14 and Q1.18. `just test` passes. Fix what fails.

**Checkpoint**: Story 1 is complete. Ships as its own PR.

---

## Phase 4: User Story 2 - Choose TCP or UDP (Priority: P2)

**Step**: 3, Transport setting · **Issue**: to be created under #179 · after Step 2

**Goal**: A setting with Automatic, TCP and UDP, and the transport in use shown in the
statistics overlay.

**Independent Test**: Play the test source with the setting on Automatic, TCP and UDP in turn and
read the transport in the statistics overlay; with the server restricted to TCP, Automatic still
plays and forced UDP fails with a message that points to Settings (quickstart Q2.1–Q2.12).

- [ ] T028 [US2] In `rtspconnection.h` / `rtspconnection.cpp`, add the setting of [data-model.md → Transport setting](./data-model.md#transport-setting): values `auto`, `tcp` and `udp`, and "an unknown stored value is read as `auto`". `open` takes the setting and plans the attempts by the five rows of [the attempts table](./contracts/rtsp-connection.md#attempts). Add the session transport memory: "a set of source keys that needed TCP under Automatic", filled when the TCP attempt of Automatic succeeds, consulted by `open`, "emptied when the user changes the transport setting", never saved. A forced transport never falls back.
- [ ] T029 [P] [US2] In `tests/test_rtspconnection.cpp`, add: one test per row of the attempts table; forced UDP fails without a TCP attempt and reports the transport and that it was forced; the memory makes the second open of the same source start with TCP, is keyed by host and port, and is emptied by a change of setting; `rtsps://` is one TCP attempt under every setting; an unknown setting value behaves as `auto`.
- [ ] T030 [US2] In `custommediaplayer.h` / `custommediaplayer.cpp`, add the writable property `rtspTransport`, read each time a stream is opened so that a change does not touch the stream that is playing (FR-010), and use the forced-transport messages of [the messages table](./contracts/rtsp-connection.md#messages) with the kind `transport`.
- [ ] T031 [US2] In `Main.qml`, add `property string rtspTransport: "auto"` to the application settings and bind the player's `rtspTransport` to it. In `SettingsDialog.qml`, add "RTSP transport" to the Playback tab with the choices Automatic, TCP and UDP.
- [ ] T032 [US2] In `NerdStatsOverlay.qml`, add a row "Transport" that is shown for RTSP streams only, with the player's `transportInUse`, and `TCP (secure stream)` for a secure address when the setting is not TCP.
- [ ] T033 [US2] In `Main.qml`, give the snackbar for the kind `transport` the action "Settings", which opens Settings on the Playback tab.
- [ ] T034 [US2] Check the step with the test source and the silent server: quickstart Q2.1–Q2.12, restarting MediaMTX with `rtspTransports` set to `[tcp]` where a row asks for it. `just test` passes. Fix what fails.

**Checkpoint**: Stories 1 and 2 work. Ships as its own PR.

---

## Phase 5: User Story 3 - Credentials stay private (Priority: P3)

**Goal**: A user name and password in an address are used to sign in and appear nowhere else.

**Independent Test**: With recent files, session restore and verbose logging on, play an address
with credentials, close GAV, and search the history file, the session playlist and the log for
the password (quickstart Q3.1–Q3.10).

### Display address everywhere (Step 1; same branch as Phase 2)

These apply to the `http` and `https` addresses GAV already accepts, so that the rule is in place
before any RTSP address is.

- [ ] T035 [US3] In `playlistmodel.h` / `playlistmodel.cpp`, add the role `displayLocation` ("the display address for a stream, the path for a file"), and make a title that is derived from an address use the display address (`fallbackTitle` returns the whole address today when it has no file name). The entry's `location` keeps the full address (FR-019). Add tests to `tests/test_playlistmodel.cpp`: the role for a file, for a stream, and for a stream with credentials; the derived title of `https://user:pw@example.org/` has no credentials; `location` still has them.
- [ ] T036 [US3] In `playlistview.cpp`, make search and sorting by path use the display location, and add a test to `tests/test_playlistview.cpp` that searching for the user name or the password of an entry finds nothing while searching for its host finds it.
- [ ] T037 [P] [US3] In `PlaylistRow.qml`, show `displayLocation` on the address line. In `Main.qml`, build the window title and the mini player title from the entry's title or display address, never from the full address.
- [ ] T038 [P] [US3] In `playbackhistory.cpp`, store the display address in `recordOpened`, and compare by display address when removing or de-duplicating. Add a test to `tests/test_playbackhistory.cpp`: after recording an address with credentials, the history file and `recentFiles()` contain it without them.
- [ ] T039 [US3] In `playlistio.cpp`, make the writer leave credentials out by default: it writes display addresses for the tags `session` and `save` alike. Add a test to `tests/test_playlistio.cpp`: a document with a credentialed entry, written with either tag, contains no credentials (FR-017, FR-021a). Writing them becomes possible only with the confirmed save of T046 and T047.
- [ ] T040 [P] [US3] In `mediasession.cpp`, use the display address for the now-playing address and for a title that falls back to the address. In `NerdStatsOverlay.qml`, show and copy the display address.
- [ ] T041 [US3] In `custommediaplayer.cpp`, use the display address in every message and log line that names the source ("Source URL is invalid", "Stream did not load within…", "MediaPlayer Error").
- [ ] T042 [US3] Check the step: quickstart Q3.9 with an `https://user:pw@…` address, with recent files and session restore on and `--verbose`: the message, `history.json`, `session.m3u8` and the output contain no `user:pw`. Then run `just run --verbose https://user:pw@127.0.0.1:9/x.mp4`: the "Loading source" line that GAV writes itself has no `user:pw` either. Save the playlist: the file has no `user:pw`. `just test` and `LC_ALL=C LANG=C just test` pass.

**Checkpoint**: Step 1 is complete (Phases 1 and 2 and this block). Ships as its own PR, before any RTSP address is accepted.

### With RTSP (Step 2; checked with the Play block of Phase 3)

- [ ] T043 [US3] Check with the test source's `cam` path: quickstart Q3.1–Q3.8 and Q3.10 (the address given on the command line), and Q4.5–Q4.10 for the save questions that ship in this step. The password `s3cr3tpw` must be found zero times in `history.json`, `session.m3u8`, the `--verbose` output, the window title, the playlist row, the statistics overlay, what its copy button copies and the operating system's now-playing display (SC-003); a wrong password gives the sign-in message after the first attempt with no TCP attempt following; a remembered address gives "This stream needs a user name and password…" and its action opens the Open URL dialog with the display address. Fix what fails.

**Checkpoint**: Story 3 works for every address GAV accepts.

---

## Phase 6: User Story 4 - RTSP entries in playlists (Priority: P4)

**Step**: 5, Playlists (with Phase 7), except T045–T047, which ship with Step 2 · **Issue**: to be created under #179 · after Step 2

**Goal**: RTSP entries are kept when a playlist is loaded, and saving a playlist that contains
passwords asks first.

**Independent Test**: Load a playlist with HTTP, RTSP and unsupported entries; the RTSP entries
are listed and play and the unsupported ones are left out; saving asks about passwords, twice
before it writes them (quickstart Q4.1–Q4.11).

RTSP entries are accepted from T012 on, because the playlist reader uses the shared rule. The
save questions (T045–T047) therefore ship in the same pull request as T012, so that no build can
write a password to a file without the user's consent (FR-021a). The rest of this phase proves
the loading behaviour.

- [ ] T044 [P] [US4] In `tests/test_playlistio.cpp`, add: a file with `rtsp://` and `rtsps://` entries keeps them with title, `group-title` and `tvg-logo`; a downloaded playlist keeps absolute RTSP entries, and resolves a relative entry against its own `http` address as before; `rtmp://`, `mmsh://` and `srt://` entries are left out and counted as unsupported; an RTSP address as a `tvg-logo` is dropped (logos stay `http` and `https`).
- [ ] T045 [US4] In `playlistmodel.h` / `playlistmodel.cpp`, add an invokable `hasPasswords()` that is true when any entry's location has a password, with tests in `tests/test_playlistmodel.cpp` for none, one, and one that is then removed.
- [ ] T046 [US4] In `playlistio.h` / `playlistio.cpp`, add the one way to keep credentials: a new tag `save-with-passwords` writes full addresses. The tags `save` and `session` keep writing display addresses (T039), and an unknown tag does too. Add to `tests/test_playlistio.cpp`: `save-with-passwords` writes the credentialed entry in full; `save`, `session` and an unknown tag do not.
- [ ] T047 [US4] In `Main.qml`, when the user saves the playlist and `hasPasswords()` is true, ask twice before any password is written ([contracts/stream-addresses.md → Saving a playlist](./contracts/stream-addresses.md#saving-a-playlist)). Both dialogs are parented to `Overlay.overlay`:
  - first: "This playlist contains passwords. Save them in the file?" with "Save with passwords", "Save without passwords" and "Cancel". "Save without passwords" saves with the tag `save`
  - second, only after "Save with passwords": "The passwords will be written to *file name* as plain text. Anyone who can read the file can use them. Save with passwords?" with "Save with passwords" and "Cancel", Cancel being the default button. Only confirming here saves with the tag `save-with-passwords`
  - Cancel in either dialog, or dismissing either, writes nothing
  - when `hasPasswords()` is false, save with the tag `save` as today, with no dialog
- [ ] T048 [US4] Check the step: quickstart Q4.1–Q4.11, with a headless, silent run for loading, search and the dialog, and a look at the written files. `just test` passes. Fix what fails.

**Checkpoint**: All four stories work.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Step**: 5, Playlists (continued)

**Purpose**: Documentation and full validation

- [ ] T049 [P] Update `README.md` (Streams section: RTSP addresses, the transport setting, that credentials in an address are not shown or saved, and that an address on the command line is visible in the process list) and `CLAUDE.md` (the two new units, `CustomMediaPlayer`'s part, `tests/rtsp/`).
- [ ] T050 Run the whole of [quickstart.md](./quickstart.md): the 30-minute run (Q1.15, SC-006), the package checks QP.1–QP.3 on Linux, and QR.1–QR.3. Record the results in the pull request description, and say plainly which of Windows, macOS and a real camera were not run.
- [ ] T051 Run `just format` and `just lint` and resolve what they report in the files this feature changed.

---

## Delivery order

Each step is one pull request. Do the steps in this order; within a step, do the tasks in
number order unless marked [P].

| Step | Pull request | Tasks | Needs |
|---|---|---|---|
| 1 | Addresses and credentials | T001–T010, T035–T042 | — |
| 2 | Play RTSP | T011–T022, T043, T045–T047 | Step 1 |
| 3 | Transport setting | T028–T034 | Step 2 |
| 4 | Reconnecting | T023–T027 | Step 2 |
| 5 | Playlists and wrap-up | T044, T048–T051 | Step 2; T050 needs steps 3 and 4 |

Steps 1 to 4 are released together. Until step 3 lands, RTSP streams use the Automatic plan with
no setting to change it.

No build writes a password to a file without the user's consent: from step 1 the playlist writer
leaves credentials out (T039), and the only way to keep them, the twice-confirmed save
(T045–T047), arrives in step 2 together with the first RTSP address a playlist can hold.

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: no dependencies
- **Foundational (Phase 2)**: no dependencies. Blocks every story
- **US1 (Phase 3)**: the Play block needs Foundational and the first block of US3 (Step 1 as a whole). The Reconnect block needs the Play block
- **US2 (Phase 4)**: needs the Play block of US1, whose state machine it extends
- **US3 (Phase 5)**: its first block needs Foundational. Its second block is a check that needs the Play block of US1
- **US4 (Phase 6)**: T045–T047 need the first block of US3 (T046 builds on T039) and ship with the Play block of US1. T044 and T048 need the Play block of US1
- **Polish (Phase 7)**: after everything

### User Story Dependencies

```text
Setup ─┐
       ├─▶ Foundational ─▶ US3 display address ─▶ US1 play ─┬─▶ US2 transport ──┐
       │                    (Step 1)              (Step 2)  ├─▶ US1 reconnect ──┼─▶ Polish
       │                                                    └─▶ US4 playlists ──┘
```

### Within Each User Story

- The state machine and its tests before the player code that drives it
- C++ before the QML that uses it
- A check task last, with the test source running

### Shared Files

`rtspconnection.cpp` and `tests/test_rtspconnection.cpp` are extended by Steps 2, 3 and 4.
`custommediaplayer.cpp` and `Main.qml` are edited in every step. Steps 3, 4 and 5 can be worked
on at the same time on separate branches, but expect to rebase the later ones.

### Parallel Opportunities

- Setup: T001, T002 and T003 together
- Foundational: T005 alongside T004 once the header exists
- Step 1: T037, T038 and T040 together (different files)
- Step 2: T011 and T014 alongside T013; T021 at any time after T012
- Steps 3, 4 and 5 after Step 2, by different people

---

## Parallel Example: Step 1

```bash
# The test source, together:
Task: "tests/rtsp/mediamtx.yml"                              # T001
Task: "tests/rtsp/silent_server.py"                          # T002
Task: "tests/rtsp/README.md"                                 # T003

# After StreamAddress (T004) and its adoption (T006–T009), together:
Task: "PlaylistRow.qml and titles use the display address"   # T037
Task: "playbackhistory.cpp stores the display address"       # T038
Task: "mediasession.cpp and the statistics overlay"          # T040
```

---

## Implementation Strategy

### MVP First

The first thing a user can use is Step 2, and it must not ship without Step 1:

1. Step 1: one definition of a stream address, display addresses everywhere, the log filter
2. Step 2: RTSP plays, with the Automatic plan, and saving a playlist asks twice before writing a password
3. Stop and validate Q1.1–Q1.9, Q3.1–Q3.8, Q3.10 and Q4.5–Q4.10

### Incremental Delivery

1. Addresses and credentials: nothing new is accepted; HTTP addresses with credentials are no longer shown or stored with them
2. Play RTSP: the feature
3. Transport setting: the choice the request asks for
4. Reconnecting: a dropped stream comes back
5. Playlists and wrap-up: the playlist loading tests, documentation, the full quickstart

Each PR passes `just test` under both locales and CI on all platforms before merge.

---

## Notes

- [P] tasks touch different files and do not wait on unfinished tasks
- The numbers in [research.md](./research.md) were measured on Linux x64 with Qt's stock plugin; Windows and macOS are first exercised by CI and by the package checks
- If Qt's wording of the open-failure message changes, only the sign-in message is affected; the wrong-password row of the quickstart (Q3.7) is the check
- UDP silently dropped by a real network was not reproduced; `tests/rtsp/silent_server.py` stands in for it
- Commit after each task or logical group, on a branch, never on `main`
