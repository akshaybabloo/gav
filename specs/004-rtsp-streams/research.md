# Research: RTSP Streams over TCP or UDP

Decisions for the plan, with what was measured. Measurements were taken on Linux x64 on
2026-10-09 against `main`, with Qt 6.12.0 (its bundled FFmpeg 9.0.1), a local MediaMTX 1.21.2
server fed by an `ffmpeg` test picture and tone, and a small script that behaves like an RTSP
server but never sends any media. The player in the measurements was a bare Qt `MediaPlayer`
without sound output, so that only Qt's behaviour was observed.

## R1. Can the player GAV already has play RTSP? (FR-001, FR-005, FR-028)

**Decision**: Yes. RTSP is played by the same `QMediaPlayer` that plays everything else. No new
playback path and no new dependency.

**Measured**

| Case | Result |
|---|---|
| `rtsp://` over TCP | Loaded after 1.4 to 1.6 s, first picture at about 1.8 s |
| `rtsp://` over UDP | Loaded after 1.2 to 1.8 s, first picture at about 1.8 s |
| `rtsps://` (self-signed certificate) | Loaded after 1.4 to 1.9 s; carried over TCP |
| Duration and seeking | Duration 0, not seekable, for every stream |
| Pause for 12 s, then play | Resumes over both transports, no error |

- GAV's existing rule for "live" (a network source with no duration that cannot be seeked,
  `CustomMediaPlayer::isLive`) is already true for these streams, so the LIVE badge and the
  disabled seek controls need no new logic.
- Subtitle lookup already runs for local files only, and the quality lookup for `http` and
  `https` only, so FR-007 holds without a change.
- Both FFmpeg builds contain what is needed: Qt's bundled one, and the vcpkg static one that
  GAV links and that the packaged plugin is built from (`ff_rtsp_demuxer`, `ff_rtp_protocol`,
  `ff_udp_protocol`, `ff_tcp_protocol`, `ff_tls_protocol` and `ff_srtp_protocol` are in
  `libavformat.a`). That was checked on Linux x64. A unit test asserts the same through the
  FFmpeg API, so CI confirms it on Linux arm64, Windows and macOS.

**Alternatives considered**: a separate RTSP client library (a new dependency, and a second
decoder path, for something the existing one does).

## R2. How the transport is chosen (FR-009, FR-010, FR-013)

**Decision**: GAV always tells Qt which transport to use for an `rtsp://` address, by setting the
environment variable `QT_FFMPEG_RTSP_TRANSPORT` to `tcp` or `udp` immediately before it sets the
player's source.

**Rationale**: In Qt 6.12.0 (`qffmpegmediadataholder.cpp`) the variable is read each time a source
is opened, not once at start-up, and is applied only when the scheme is exactly `rtsp`. That
makes it a per-open switch, which is what a setting that applies "from the next stream opened"
needs. It was confirmed against the server's log: `tcp` gave a TCP session, `udp` a UDP session.

For `rtsps://` Qt does not consult the variable and FFmpeg uses TCP: with `udp` requested, the
server still reported a TCP session. FR-013 therefore needs no code beyond reporting TCP as the
transport in use.

**Alternatives considered**

- Leaving the variable unset and letting FFmpeg choose (see R3).
- Options in the address (`?tcp`): removed from FFmpeg years ago.
- A patch to the packaged plugin: the stock plugin used in development would then behave
  differently from the packaged one.

## R3. Automatic: who does the fallback (FR-012, FR-012a, FR-011)

**Decision**: GAV does it. With Automatic, GAV opens the stream with UDP forced; if that attempt
fails or has shown nothing after 5 seconds, it opens it again with TCP forced. It never leaves
the choice to FFmpeg.

**Rationale**: With the variable unset, FFmpeg has the same behaviour built in. It was measured:
against a server that refuses UDP it switched at once and the stream was up in 2.1 s; against the
server that sends nothing it logged "UDP timeout, retrying with TCP" after exactly the network
timeout. But Qt gives no way to ask which transport FFmpeg ended up on, and the spec needs that
in two places: the statistics overlay (FR-011), and remembering for the session that a source
needed TCP (FR-012a). When GAV forces each attempt, it knows the transport because it chose it.

The source that "needed TCP" is remembered by host and port, in memory only.

**Alternatives considered**: reading FFmpeg's log lines to learn what it did. They are free text
and not part of any interface.

## R4. When has an attempt failed? (FR-012, FR-014, FR-027, SC-002)

**Decision**: Every RTSP attempt runs with a network timeout of 5 seconds
(`QPlaybackOptions::setNetworkTimeout`, set before the source). An attempt has succeeded when the
first picture arrives or the playback position starts to move. It has failed when:

| Signal | Applies to |
|---|---|
| The player reports an error before the first picture | Every attempt |
| 5 seconds pass without a first picture | The UDP attempt of Automatic only |
| 12 seconds pass without a first picture and without an error | The last attempt, as a backstop |

No error text is matched. When no media arrives, the error the last attempt ends on is the
player's "Demuxing failed", described below; it is handled like any other error.

**Rationale**: "The player says it loaded" cannot be the test. Against the server that completes
the handshake and then sends nothing, Qt reported the media as *loaded* and *playing*, with no
error, as soon as the network timeout ran out, and showed nothing:

| Network timeout 5 s, no media ever arrives | UDP forced | TCP forced |
|---|---|---|
| Loaded and "playing" reported at | 5.0 s | 5.1 s |
| First error ("Demuxing failed") at | 10.2 s | about 10 s |

The "Demuxing failed" error means that no packet arrived for a whole timeout period. That is the
reliable sign of a dead stream, and it does not fire for a stream that is delivering packets but
has not reached a key frame yet, which some cameras take several seconds to send. So the last
attempt waits for it rather than for a fixed time, and a slow camera is not cut off.

The UDP attempt of Automatic is the exception: the spec gives it 5 seconds. If a working UDP
stream is slow to show its first picture, the cost is only that it is played over TCP instead.

Worst cases: Automatic 5 s + 10 s = 15 s; forced 10 s. Both meet SC-002. An address that cannot
be reached at all fails faster, with the player's own error at 5 s or less.

GAV's existing 120-second load timeout for HTTP streams is left as it is and does not apply to
RTSP.

**Alternatives considered**: one fixed deadline for every attempt (cuts off cameras with long
key-frame intervals); trusting the loaded state (shown above to be wrong).

## R5. Telling a refused sign-in from other failures (FR-020, FR-027b)

**Decision**: When an attempt fails, GAV looks at the description FFmpeg gave for the failure. Qt
logs it as a warning in the category `qt.multimedia.ffmpeg.mediadataholder` ("Could not open
media. FFmpeg error description: …"), and GAV's message handler, which already receives every Qt
message, keeps the most recent one. A small function maps that text to a kind of failure:

| Description contains | Kind | What GAV does |
|---|---|---|
| `401` or `403` (`Server returned 401 Unauthorized (authorization failed)`) | Sign-in | Stops; no second transport, no reconnect; sign-in message |
| `404` or `400` | Not found | Stops; "nothing at this address" message |
| Anything else, or nothing captured | Other | Automatic goes on to TCP; otherwise the generic or transport message |

**Rationale**: Qt's own error is the same for every failure to open: code `ResourceError`, text
"Could not open file". It was the same for a wrong password, a missing path, a refused
connection and a refused transport. The description is the only place the reason appears.

Only the HTTP-style status texts are matched. They are constants inside FFmpeg, in English
whatever the locale. Texts that come from the operating system ("Connection refused",
"Protocol not supported") are translated on some systems, so nothing depends on them; those cases
fall into "Other", which is the right handling for them anyway. A unit test builds the texts from
the linked FFmpeg (`av_strerror` of `AVERROR_HTTP_UNAUTHORIZED` and the others) and checks the
mapping, so an FFmpeg update that changed them would fail a test rather than a user.

If the text is ever not captured, the result is "Other": the user gets the generic message
instead of the sign-in one. Nothing breaks.

Recognising a refused sign-in on the first attempt also matters for a reason beyond the message:
with Automatic, GAV would otherwise repeat the wrong password over TCP, and each repeat counts
towards the lock-out that many cameras apply after a handful of failed sign-ins.

**Alternatives considered**

- Opening the address a second time with GAV's own FFmpeg in the probe subprocess, to get the
  error code rather than its text. It is independent of Qt's wording, but it signs in again with
  the same wrong password (see lock-out above) and delays the message.
- Patching the packaged plugin to put the description in the error text: the development build
  would behave differently from the packages.

## R6. What a dropped stream looks like, and reconnecting (FR-027a, FR-027b, SC-009)

**Decision**: GAV treats an RTSP stream that was playing as lost when any of these happens while
the user has not stopped or paused it:

| Signal | Measured |
|---|---|
| The player reports the end of the media, for a source that reported no duration | The publisher was stopped: end of media 1.2 s later, state "stopped", **no error** |
| The player reports an error | The server was frozen: "Demuxing failed" after two timeout periods, then again every period, while the state stayed "playing" |
| The position has not moved for 5 seconds | In the frozen case the position stopped within a second |

It then reconnects up to 3 times: attempt *n* starts 2, 9 and 16 seconds after the loss, or when
the previous attempt ends if that is later, and each attempt is limited as in R4 with the
transport that was in use. During this the player reports a "reconnecting" state for the
interface, and does not report the end of the media, so that the playlist does not advance to
the next entry. If all three fail, it reports the stream as lost and stops.

**Rationale**: A live stream has no end, so "end of media" on one is always a loss. An RTSP
source can also serve a recording, which reports a duration; its end of media is its real end,
and reconnecting there would play the recording again from the start. So the duration decides.
Such a source is still shown as live, as the spec asks, because the existing rule for "live"
looks at the duration and would otherwise give it a seek bar that GAV does not support for
RTSP. The spacing
is chosen so that a source that is away for up to 10 seconds is picked up by an attempt within
7 seconds of coming back, plus about 2 seconds to load (SC-009), and so that the three attempts
are over about 30 seconds after the loss, as the clarification asks.

A stream that is lost while paused is not retried in the background. Pressing play opens it
again.

Reconnecting never starts for a stream that had not yet shown a picture (that is a failed open,
R4), and stops for good on a sign-in failure (R5).

**Alternatives considered**: retrying without limit (out of scope by the spec); relying on the
player's error alone (the clean end-of-media case has none).

## R7. Keeping the password out of the log (FR-018)

**Decision**: One function removes the user name and password from any address inside a piece of
text, and it is applied inside the logger itself: the logger's output goes through a filtering
sink that rewrites every line before it reaches the console. The application's logger and the
probe subprocess's logger both use it.

**Measured**: the password does reach the log today. With the logging GAV turns on
(`QT_FFMPEG_DEBUG`), FFmpeg's description of the opened stream arrives as
`FFmpeg log: Input #0, rtsp, from 'rtsp://viewer:…@127.0.0.1:8554/cam':`. GAV's own
"stream did not load" warning also prints the whole address. Those two pass through Qt's message
handler. But GAV also writes to the logger directly, without going through that handler: for
example `Loading source: '…'` and `Parsed as URL: '…'` in `main.cpp`, the probe's argument list
in `mediaprobe.cpp`, and a subtitle source in `subtitlecontroller.cpp`. A filter in the message
handler alone would miss those. Every one of these paths ends in the same `spdlog` logger, so
the logger is the one point that covers Qt, FFmpeg and GAV's own lines together.

The command line prints an address back when it is refused ("Unsupported address …"); that goes
through the same function.

**Rationale**: Filtering at the single place every line passes is the only way to cover library
output GAV does not write itself, and it does not depend on each log call remembering to use the
display address. Log calls that name an address still use the display address where they are
touched, but the sink is what guarantees the result.

**Alternatives considered**: filtering in Qt's message handler (misses GAV's direct log calls,
as found in the analysis); fixing each log call by hand (nothing stops the next one from being
written without it).

**Limit**: an address given on the command line is visible to other programs on the machine in
the process list, as with any command-line argument. That is outside what GAV writes or shows
and is noted in the documentation.

## R8. One definition of "an address GAV plays" (FR-001, FR-003, FR-016, FR-017, FR-022, FR-024)

**Decision**: A new small unit, `StreamAddress`, holds the address rules, and every place that
decides them today calls it:

- which schemes are streams (`http`, `https`, `rtsp`, `rtsps`), and which of those are RTSP
- the display address: the address without user name and password
- whether an address carries a password
- the text filter of R7

**Rationale**: The rule "is this a stream" is written out separately in nine places today
(`main.cpp`, `playlistio.cpp`, `playlistmodel.cpp`, `playbackhistory.cpp`, `mediasession.cpp`,
`mediasession_mpris.cpp`, `Main.qml`, `RecentFilesMenu.qml`, `OpenUrlDialog.qml`). Adding two
schemes in nine places invites missing one, and the constitution asks for the supported schemes
to be explicit.

Not every `http` check changes. Fetching a playlist document, a logo, an HLS playlist or a
subtitle file stays `http` and `https` only.

Credentials are kept in memory on the playlist entry and on the player's source, so replaying
within a session works (FR-019). Everything that leaves memory or reaches the screen uses the
display address: the playlist row, the window title, titles derived from the address, recent
files, the session playlist, the statistics overlay and its copy action, messages, and the
operating system's now-playing data.

## R9. Saving a playlist that contains passwords (FR-021)

**Decision**: The save action checks whether any entry carries a password. If one does, a dialog
asks, with "Save with passwords", "Save without passwords" and "Cancel". Choosing "Save with
passwords" opens a second dialog that says the passwords will be written as plain text, with
"Cancel" as its default button; only confirming that one writes them. The session playlist that
GAV writes by itself never asks and always writes display addresses.

The playlist writer leaves credentials out unless it is told explicitly to keep them. Keeping
them is therefore something only the confirmed dialog can ask for, not something that happens
when a caller forgets a flag.

This ships in the same step that first lets a playlist hold an RTSP address, so that no build
can write a password without the two confirmations.

**Rationale**: The clarifications: passwords may be written, with express consent, asked twice.

## R10. Remote playlists and RTSP entries (FR-023, Constitution II)

**Decision**: RTSP entries are kept from playlists loaded from a file and from an address alike.

**Rationale**: The constitution says content fetched from the network must not make GAV open
address schemes it does not explicitly support. RTSP becomes one of the explicitly supported
ones, through the list in R8. A downloaded playlist can name a device on the user's own network;
that is equally true of its `http` entries today. No connection is made until an entry is
played, and logos stay `http` and `https` only, so a list cannot make GAV contact an RTSP device
just by being loaded.

## R11. Where the setting and the new states surface (FR-009, FR-011, FR-027a)

**Decision**

- Setting: "RTSP transport" with Automatic, TCP and UDP, in the Playback tab of Settings, stored
  with the other preferences under `rtspTransport` (`auto`, `tcp`, `udp`; default `auto`).
- Transport in use: a row in the statistics overlay, shown for RTSP streams only.
- Connecting and reconnecting: the existing buffering indicator, with the text "Reconnecting…"
  while reconnecting.
- Messages: the existing snackbar. The sign-in message has an "Open URL" action that opens the
  dialog with the display address filled in, which settles the point the clarification left
  open.

**Rationale**: Nothing is added to the main playback view (Constitution I).

## R12. How this is tested (Constitution V, SC-001 to SC-009)

**Decision**

- **Unit tests** for everything that needs no network: the address rules and the text filter;
  the connection logic, written as a state machine that is fed events (first picture, error,
  time passing, end of media) and returns actions (open with this transport, report this
  message), so every row of R4 and R6 is a test; the failure mapping of R5; playlists keeping
  RTSP entries and writing display addresses; history; and the FFmpeg capability check of R1.
- **A local RTSP source** for the rest: MediaMTX 1.21.2 with a configuration file and an `ffmpeg`
  publisher, plus the no-media server script, both kept under `tests/rtsp/`. The server binary
  is downloaded by whoever runs the quickstart and is not committed.
- **Headless runs** of the application against that source, silent, for the scenarios that can
  be driven without a person.
- **By hand**: the packages on Windows and macOS (SC-007), and a real camera if one is available.

**Rationale**: The parts most likely to be wrong are the decisions about attempts and timeouts.
Keeping them out of the player class, in a unit with no Qt Multimedia in it, is what makes them
testable in CI on every platform.

UDP that is silently dropped by a network could not be reproduced with a firewall rule here
(that needs administrator rights). The no-media server reproduces the same thing from the
client's point of view.

## R13. Not doing

- **Low-latency mode.** Qt has a playback option that turns off FFmpeg's input buffering. It was
  not measured and is not asked for. Left for a later change.
- **Other schemes** (`rtmp`, `mmsh`, `srt`): out of scope by the spec. They would be one line
  each in R8 plus their own testing.
