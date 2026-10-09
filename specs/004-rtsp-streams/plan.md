# Implementation Plan: RTSP Streams over TCP or UDP

**Branch**: `004-rtsp-streams` (spec directory; work branches are cut from GitHub issues, starting from #179) | **Date**: 2026-10-09 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `specs/004-rtsp-streams/spec.md`

## Summary

GAV plays `rtsp://` and `rtsps://` addresses as live streams, lets the user choose how they are
carried, and keeps a user name and password in an address out of everything it shows, logs or
saves.

1. **Watch**: RTSP addresses are accepted in the Open URL dialog, on the command line, from a
   second launch and from the operating system, and play as live streams. A stream that drops is
   reconnected up to three times.
2. **Transport**: a setting with Automatic (UDP, then TCP), TCP and UDP. The statistics overlay
   shows the one in use.
3. **Credentials**: used to sign in and for nothing else. Everything outside the connection sees
   the address without them.
4. **Playlists**: RTSP entries are kept, and saving a playlist that contains passwords asks
   twice before it writes them.

**Technical approach** (from [research.md](./research.md))

- No new playback path and no new dependency. The `QMediaPlayer` GAV already uses plays RTSP over
  both transports, and both FFmpeg builds (Qt's and the vcpkg one behind the packaged plugin)
  contain what is needed. The existing rule for "live" already matches these streams.
- GAV chooses the transport for every open, through the environment variable Qt reads at each
  open, and decides the Automatic fallback itself. That way it always knows the transport in
  use, which Qt cannot report.
- Success and failure of an attempt are decided by GAV, not by the player's "loaded" state:
  measurements showed Qt reporting a stream as loaded and playing when no media ever arrived.
- The decisions about attempts, time limits and reconnecting live in a new unit with no playback
  code in it, a state machine fed with events, so that all of it is unit-tested on every
  platform.
- A refused sign-in is recognised from the failure description FFmpeg gives, which GAV's message
  handler already receives. Only FFmpeg's own fixed status texts are matched.
- One new unit, `StreamAddress`, defines which addresses are streams and what their display
  form is. The nine places that each decide this today call it instead. Its text filter runs
  inside the logger, the one point every log line passes, including the lines GAV writes
  directly rather than through Qt's message handler.
- The playlist writer leaves credentials out unless a save confirmed twice by the user asks for
  them, and that save ships in the same step that first lets a playlist hold an RTSP address.
- Delivery is in five pull requests. Credentials handling lands first, so that `main` never
  holds a build that accepts camera addresses and leaks their passwords.

## Technical Context

**Language/Version**: C++20 and QML (Qt 6.12).

**Primary Dependencies**: Qt Multimedia with the FFmpeg backend (playback), FFmpeg 9.x from vcpkg (linked into `gav`, and the packaged plugin). Nothing new. For testing only, not shipped: MediaMTX 1.21.2 and the `ffmpeg` command.

**Storage**: One new preference, `rtspTransport`, in Qt Settings. `history.json` and `session.m3u8` keep their formats; RTSP addresses are written to them without credentials.

**Testing**: Google Test through `just test`, also under `LC_ALL=C LANG=C`. A local RTSP source for the scenarios in [quickstart.md](./quickstart.md), driven by silent headless runs where possible.

**Target Platform**: Linux (x64 and arm64), Windows, macOS.

**Project Type**: Desktop application (single CMake project, C++ and QML files at the repository root).

**Performance Goals**: First picture within 5 s on a local network (measured: about 1.8 s); Automatic with UDP blocked within 10 s the first time; a failure message within 15 s; a dropped stream playing again within 10 s of the source returning (SC-001, SC-002, SC-009).

**Constraints**: Opening must not block the interface (Qt opens sources off the UI thread; attempts are cancellable). Every attempt has a time limit. The password must not reach the screen, the log, any file GAV writes by itself, or the operating system's media controls. HTTP, HTTPS and local playback must not change.

**Scale/Scope**: Two new C++ units and their tests, changes in about ten existing C++ and QML files, one new setting, one new dialog, about ten user-visible texts, three test-support files. No change to file formats.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.* Checked against
constitution v1.3.0.

| Principle | Gate | Before research | After design |
|---|---|---|---|
| I. Simple, Focused Player | Features serve playing media. Network only for user-initiated actions, with a timeout, stopping when the media is closed. What is remembered holds the least identifying data. No clutter in the playback view. | PASS, with two points to resolve: are reconnect attempts "user-initiated", and where does a password end up? | PASS. A stream is opened by the user. Reconnecting is read as one of "the requests needed to play it": it re-opens the same address the user opened, is limited to three attempts in about 30 s, and stops when the stream is closed. The principle's list of what opening a stream covers does not name reconnecting, so this is a reading, recorded here for review; if it is not accepted, the constitution needs a clarifying amendment before step 4. The "size limit" the principle asks for applies to the extra requests around a stream; the stream itself has none, as with the HTTP live streams GAV already plays, and RTSP adds no extra request. Every attempt has a 5 s network timeout and a decided end (R4, R6). Recent files and the session playlist hold the address without credentials; no password is stored (R8). The transport is a setting; the transport in use is in the statistics overlay; nothing is added to the playback view (R11). |
| II. Responsive UI Through Isolation | Slow work off the UI thread. Network content must not make GAV open schemes it does not explicitly support. Failures surfaced as a clear message. | PASS, with one point to resolve: RTSP entries in downloaded playlists. | PASS. Opening is asynchronous in Qt and cancellable. RTSP becomes an explicitly supported scheme, defined in one place (R8); a downloaded playlist causes no RTSP connection until an entry is played (R10). Each failure has its own message (rtsp-connection contract). Playback stays in the main process, as for every other source today. |
| III. Cross-Platform Parity | Same behaviour on all platforms; packaging keeps working. | PASS, with one point to resolve: is RTSP in the packaged FFmpeg everywhere? | PASS. The same vcpkg FFmpeg port builds all packages; a unit test checks the RTSP demuxer and the TCP, UDP and TLS protocols through the FFmpeg API, so CI proves it per platform (R1). The quickstart has package checks for all three systems. No platform-specific code is added. |
| IV. Pinned, Patchable Dependencies | No reliance on Qt's bundled FFmpeg; new dependencies justified. | PASS | PASS. No new dependency. The behaviour relied on (the transport variable, the playback options) is in the Qt tag the packaged plugin is built from. MediaMTX is a pinned test tool that is not shipped. |
| V. Tested Core Logic | Non-visual logic has tests; UI has quickstart steps; CI green on all platforms. | PASS | PASS. Address rules, the text filter, the connection state machine, the failure mapping, playlist and history handling are unit-tested (R12). Everything that needs a server or a screen is in the quickstart. |
| Technology Constraints | Qt Multimedia with FFmpeg; `spdlog` logging, quiet normal runs; preferences in Qt Settings. | PASS | PASS. The log filter is a sink inside the existing `spdlog` logger. The setting uses Qt Settings. |
| Development Workflow | Issues and `<issue>-<slug>` branches; one usable increment per pull request; docs updated in the same change. | PASS | PASS. Five pull requests under #179 (below). `README.md` and `CLAUDE.md` are updated with the step that changes behaviour. |

**Result**: Gates pass with no deviations. No unresolved clarifications remain.

## Project Structure

### Documentation (this feature)

```text
specs/004-rtsp-streams/
├── plan.md              # This file
├── research.md          # Phase 0: decisions R1–R13, with measurements
├── data-model.md        # Phase 1: addresses, the setting, the connection state machine
├── quickstart.md        # Phase 1: validation against a local RTSP source
├── contracts/
│   ├── stream-addresses.md   # Accepted addresses, display form, credentials, saving
│   └── rtsp-connection.md    # Attempts, transport, messages, reconnecting
├── checklists/
│   └── requirements.md
└── tasks.md             # Phase 2 output (/speckit-tasks; not created by /speckit-plan)
```

### Source Code (repository root)

```text
+ streamaddress.h / .cpp        # Which addresses are streams, display address, password check,
                                # source key, and the filter that removes credentials from text
+ rtspconnection.h / .cpp       # The connection state machine: attempts, limits, fallback,
                                # reconnect schedule, failure kinds. No Qt Multimedia in it
~ custommediaplayer.h / .cpp    # Drives the state machine for RTSP sources: sets the transport
                                # and timeout before each open, watches for the first picture,
                                # loss and stall; new properties rtspTransport, transportInUse,
                                # reconnecting, displaySource
+ credentialfiltersink.h        # Logger sink that removes credentials from every line
~ main.cpp                      # Accept rtsp/rtsps on the command line; loggers built on the
                                # filter sink; message handler keeps the last open-failure
                                # description
~ playlistio.h / .cpp           # Keep rtsp/rtsps entries; write display addresses always,
                                # except for a save the user confirmed twice
~ playlistmodel.h / .cpp        # rtsp/rtsps entries are streams; display location role;
                                # titles made from the display address
~ playlistview.cpp              # Search and sort use the display location
~ playbackhistory.cpp           # Recent files hold the display address; no resume for RTSP
~ mediasession.cpp              # Now-playing uses the display address; accepts rtsp to open
~ mediasession_mpris.cpp        # Supported schemes
~ playbackutils.h / .cpp        # Exposes the address rules to QML
~ Main.qml                      # Uses the shared rule; setting; messages and their actions;
                                # the two save-with-passwords dialogs
~ OpenUrlDialog.qml             # Accepts rtsp/rtsps; texts; can be opened pre-filled
~ SettingsDialog.qml            # "RTSP transport" in the Playback tab
~ NerdStatsOverlay.qml          # Transport row; display address
~ MediaComponent.qml            # "Reconnecting…" with the buffering indicator
~ MediaControlsComponent.qml    # End-of-media handling checked for RTSP
~ PlaylistRow.qml, RecentFilesMenu.qml   # Show the display address
~ CMakeLists.txt                # New sources in appgav and gav_tests
+ tests/test_streamaddress.cpp  # Also the log filter
+ tests/test_rtspconnection.cpp
+ tests/test_rtspsupport.cpp    # RTSP and its protocols are in the linked FFmpeg
~ tests/test_playlistio.cpp, test_playlistmodel.cpp, test_playlistview.cpp,
  test_playbackhistory.cpp, test_playbackutils.cpp
~ .gitignore                    # The test certificate under tests/rtsp/
+ tests/rtsp/mediamtx.yml, silent_server.py, README.md   # Test source for the quickstart
~ README.md, CLAUDE.md          # RTSP, the transport setting, the credentials rule
```

`+` new, `~` changed.

**Structure Decision**: Single project, flat layout, as before. Two new units follow the pattern
of `ShuffleOrder` and `HlsMaster`: logic with no user interface in it, compiled into both the
application and the test binary. `CustomMediaPlayer` stays the only class that touches the
player.

## Delivery order

Each step is its own branch and pull request and leaves `main` building and passing. Issue #179
is the parent; sub-issues are created with the tasks. Stories 1 to 3 are not released until all
of steps 1 to 4 are merged.

| Step | Content | Spec coverage | Visible change |
|---|---|---|---|
| 1. Addresses and credentials | `StreamAddress`; the nine scheme checks call it (still `http` and `https` only); display address everywhere it is shown or saved, including every playlist file GAV writes; the log filter in the logger. | FR-015 to FR-019, FR-021a, FR-022 (Story 3 for the addresses GAV already accepts) | HTTP addresses with credentials no longer show or store them |
| 2. Play RTSP | `rtsp` and `rtsps` become stream schemes; `RtspConnection` with the Automatic plan built in; first-picture and failure decisions; failure kinds and messages; Open URL texts; the twice-confirmed save with passwords. | FR-001 to FR-008, FR-014, FR-020, FR-021, FR-026, FR-027, FR-028 (Story 1 except reconnecting; Story 3 complete) | RTSP addresses play |
| 3. Transport setting | The setting; forced TCP and UDP; the session's TCP memory; the statistics overlay row. | FR-009 to FR-013 (Story 2) | Settings → Playback → RTSP transport |
| 4. Reconnecting | Loss detection, the three attempts, the "reconnecting" state, no playlist advance. | FR-027a, FR-027b (rest of Story 1) | A dropped stream comes back by itself |
| 5. Playlists | Tests and checks for RTSP entries kept from files and addresses. Documentation; the full quickstart, including packages. | FR-023 to FR-025 (Story 4), SC-001 to SC-009 | RTSP entries in playlists are verified |

Step 1 comes before the story the spec ranks first because it is what makes step 2 safe to
merge: from the first build that accepts a camera address, its password is already kept out of
the log, the history, the screen and every file GAV writes, unless the user confirms twice that
a saved playlist should contain it. Steps 3, 4 and 5 do not depend on each other.

## Risks

| Risk | Mitigation |
|---|---|
| Qt's wording of the open-failure message changes in a later Qt version, and a refused sign-in is no longer recognised. | Only the sign-in message depends on it; everything else works from the player's own signals. The fallback is the generic message. The quickstart has a wrong-password check, and a unit test pins FFmpeg's side of the text (R5). |
| The transport variable is process-wide, and two players opening RTSP at the same moment could read each other's value. | Only the main player opens network sources; the preview player is for local files. The variable is set immediately before the open. |
| A camera takes longer than 5 s to show a first picture over UDP. | Under Automatic it is then played over TCP, which is correct if not ideal. The last attempt waits for data, not for a fixed time (R4). |
| A real network drops UDP silently, which could not be reproduced here. | The no-media server gives the client the same view. A real-network check is listed for whoever has a blocked network or a camera. |
| Reconnecting hides a stream that is really gone. | Three attempts, about 30 s, then a message. |
| Secure RTSP fails on one platform's package because of how TLS is built there. | The capability test covers the TLS protocol on every CI platform; the quickstart plays a secure stream from each package. |
| The credentials rule misses a place that shows an address. | The places are listed in the stream-addresses contract; the quickstart searches every file GAV writes and its verbose output for the password (SC-003). |

## Complexity Tracking

No violations.
