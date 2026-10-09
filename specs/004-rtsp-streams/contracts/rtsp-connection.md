# Contract: Connecting, Transport and Reconnecting

How an RTSP stream is opened, which transport carries it, and what happens when it fails or
drops. Covers FR-004 to FR-014, FR-020, FR-026 to FR-027b, SC-001, SC-002, SC-004 and SC-009.
The measurements behind the numbers are in [research R4 and R6](../research.md).

## The setting

Settings → Playback → **RTSP transport**: Automatic (default), TCP, UDP. Stored as
`rtspTransport` = `auto` | `tcp` | `udp`. It is read when a stream is opened, so a change applies
to the next stream and never interrupts the one that is playing.

## Attempts

Every attempt uses a network timeout of 5 seconds.

| Setting | Address | Attempts, in order |
|---|---|---|
| Automatic | `rtsp://`, source not in the session's TCP memory | UDP, then TCP |
| Automatic | `rtsp://`, source in the session's TCP memory | TCP |
| TCP | `rtsp://` | TCP |
| UDP | `rtsp://` | UDP |
| Any | `rtsps://` | TCP |

**An attempt succeeds** when the first picture arrives or, for a stream without picture, when
the position starts to move.

**An attempt fails** when:

| Condition | Which attempts |
|---|---|
| The player reports an error before success | All |
| 5 seconds pass without success | The UDP attempt of Automatic |
| 12 seconds pass without success | The last attempt |

When no media arrives at all, the player's error comes after two timeout periods, about 10
seconds into the attempt, so the last attempt normally ends there and the 12 seconds are a
backstop.

When the UDP attempt of Automatic fails for any reason other than a sign-in or "not found", the
TCP attempt follows without a message. When that TCP attempt succeeds, the source is added to
the session's TCP memory.

Closing the stream or opening something else ends the current attempt at once and cancels any
that would follow.

**Longest time to a failure message**: 15 seconds with Automatic, 10 seconds with a forced
transport.

## While connecting

The buffering indicator is shown from the open until the first picture, whatever the underlying
player reports: it says "loaded" and "playing" when no media has arrived, and the interface
must not show that as a playing stream. The entry is the current one in the playlist. The
controls that work for a live stream are available; pressing stop or choosing another entry
cancels.

## Live

Every RTSP stream is shown as live, with the LIVE badge and without seeking, whatever duration
the source reports (FR-005). A source that reports a duration is still played to its end: when
it ends, it ends like any other entry, the playlist moves on, and nothing is reconnected.

## Transport in use

The statistics overlay has a row **Transport** for RTSP streams, with the value `TCP` or `UDP`.
For a secure address under a UDP or Automatic setting the value is `TCP (secure stream)`.

## Messages

All are shown in the snackbar and name the stream by its display address or title.

| Situation | Message | Action |
|---|---|---|
| Automatic, both attempts failed; or secure address failed | "Could not open the stream. Check the address and your connection." | — |
| Forced UDP failed | "Could not open the stream over UDP. Choose Automatic or TCP in Settings." | Settings |
| Forced TCP failed | "Could not open the stream over TCP. Choose Automatic or UDP in Settings." | Settings |
| Sign-in refused, the address had credentials | "Signing in to the stream failed. Check the user name and password." | Open URL |
| Sign-in needed, the address had none | "This stream needs a user name and password. Open it as rtsp://user:password@…" | Open URL |
| Nothing at the address | "The source has no stream at this address." | — |
| All reconnect attempts failed | "The stream was lost." | — |

The **Open URL** action opens the dialog with the display address filled in. The **Settings**
action opens Settings on the Playback tab.

## Reconnecting

Starts only for an RTSP stream that had been playing and that the user has not stopped or
paused, when one of these happens:

- the player reports the end of the media, and the source had reported no duration
- the player reports an error
- the position has not moved for 5 seconds

| | |
|---|---|
| Attempts | Up to 3 |
| Attempt *n* starts | 2, 9 and 16 seconds after the loss, or when the previous attempt ends if that is later |
| Each attempt | As a last attempt above, with the transport that was in use and the entry's credentials |
| During | The buffering indicator with the text "Reconnecting…"; the stream stays the current entry; the playlist does not advance |
| An attempt succeeds | Playback carries on as live; the indicator goes away; no message |
| Sign-in refused during an attempt | Reconnecting stops; the sign-in message is shown |
| All three fail | "The stream was lost."; the player stops; the playlist does not advance |
| Longest time from the loss being noticed to that message | About 38 seconds: attempts start at 2, 14 and 26 seconds when each one runs to its 12-second limit |
| The user closes the stream or opens something else | Reconnecting stops at once, with no message |
| The stream is lost while paused | No attempts. Pressing play opens the stream again |

## What the player exposes to the interface

| Property | Type | Meaning |
|---|---|---|
| `rtspTransport` | text, writable | The setting (`auto`, `tcp`, `udp`) |
| `transportInUse` | text | `TCP`, `UDP` or empty |
| `reconnecting` | true or false | A reconnect attempt is in progress or scheduled |
| `displaySource` | address | The display address of the current source |

The existing `mediaLoaded` and `errorOccurred` keep their meaning. For an RTSP source, `live` is
always true, and `buffering` is true while connecting and while reconnecting. While
reconnecting, `live` and `mediaLoaded` stay true and the end of the media is not reported.

A failure is reported with the signal `rtspFailed(kind, message)`:

| `kind` | When | Message row |
|---|---|---|
| `signIn` | The source refused the credentials in the address | Sign-in refused |
| `signInNeeded` | The source asked for a sign-in and the address had no credentials | Sign-in needed |
| `notFound` | The source answered that nothing is at the address | Nothing at the address |
| `transport` | A forced transport failed for any other reason | Forced UDP failed, forced TCP failed |
| `other` | Automatic or a secure address failed for any other reason | Automatic, both attempts failed |
| `lost` | All reconnect attempts failed | All reconnect attempts failed |

## Unchanged

- HTTP and HTTPS streams: how they open, time out and end (SC-008).
- Local files.
- Resume positions: never stored or offered for an RTSP stream (FR-006).
