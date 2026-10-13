# Data Model: RTSP Streams over TCP or UDP

What the feature holds in memory and what it persists. Behaviour is in
[contracts/](./contracts/); reasons are in [research.md](./research.md).

## Stream address

A network address GAV can play.

| Field | Notes |
|---|---|
| scheme | `http`, `https`, `rtsp` or `rtsps`, compared without regard to case |
| host, port, path, query | As given. The port may be absent |
| user name, password | Optional. Either may be present without the other |

**Derived values**

| Value | Rule |
|---|---|
| is stream | The scheme is one of the four |
| is RTSP | The scheme is `rtsp` or `rtsps` |
| is secure RTSP | The scheme is `rtsps` |
| has password | A password is present and not empty |
| display address | The same address with user name and password removed. Everything else, including percent-encoding, is unchanged |
| source key | Host in lower case, and the port (554 for `rtsp`, 322 for `rtsps` when absent). Identifies "the same source" for the session's transport memory |

**Rules**

- The full address exists only in memory: on the playlist entry and as the player's source.
- The display address is the only form that is shown, logged or saved by GAV itself (FR-016 to
  FR-018). The one exception is a playlist file the user saves after confirming twice that it
  should contain the passwords (FR-021).
- An address whose scheme is not one of the four is not a stream address and is refused
  (FR-003, FR-024).

## Transport setting

The user's choice. One value for the application.

| Value | Stored as | Meaning |
|---|---|---|
| Automatic | `auto` | UDP first, then TCP. The default |
| TCP | `tcp` | TCP only |
| UDP | `udp` | UDP only |

Stored with the other preferences under the key `rtspTransport`. An unknown stored value is read
as `auto`. Read each time a stream is opened (FR-010).

## Transport in use

`TCP` or `UDP`, for the RTSP stream that is playing; empty otherwise. Set when an attempt
succeeds. Always `TCP` for a secure RTSP address (FR-013).

## Session transport memory

A set of source keys that needed TCP under Automatic. In memory only; empty at start; never
saved (FR-012a). Emptied when the user changes the transport setting.

## Connection

The life of one RTSP stream in the player. A state machine with no playback code in it.

```text
              open                    first picture
   Idle ─────────────▶ Connecting ─────────────────▶ Playing
     ▲                  │   ▲                          │
     │  failed, no      │   │ next transport           │ lost (end of media, error,
     │  attempt left    │   │ (Automatic, UDP→TCP)     │ or position still for 5 s)
     │                  ▼   │                          ▼
     ├──────────────── Failed                      Reconnecting ──▶ Playing   (attempt succeeds)
     │                                                 │
     └─────────────────────────────────────────────────┘ third attempt fails → Lost
   Any state ── user closes the stream or opens something else ──▶ Idle
```

| State | Fields |
|---|---|
| Connecting | transport being tried; whether another transport may follow; time started |
| Playing | transport in use |
| Reconnecting | transport in use; attempt number (1 to 3); time of the loss |
| Failed, Lost | the failure kind, for the message |

**Rules**

- Attempts, limits and what counts as success or failure: [research R4](./research.md) and
  [contracts/rtsp-connection.md](./contracts/rtsp-connection.md).
- Reconnecting is entered only from Playing (FR-027b).
- A sign-in failure ends in Failed from any state, with no further attempt (FR-020, FR-027b).
- A stream lost while paused goes to Idle with its entry kept; play opens it again.
- The end of the media is a loss only when the source had reported no duration. A source with a
  duration that reaches its end goes to Idle and is not reconnected.

## Open failure

Why an attempt failed. Derived, never stored.

| Kind | From |
|---|---|
| Sign-in | The source answered 401 or 403 |
| Not found | The source answered 404 or 400 |
| Other | Anything else, including an address that cannot be reached, a refused transport, and a connection over which no media arrived |

How a kind becomes a message, together with the setting and whether the address had credentials,
is in [contracts/rtsp-connection.md](./contracts/rtsp-connection.md#what-the-player-exposes-to-the-interface).

## Playlist entry (changed)

The existing entry, with one change in meaning and one new derived value.

| Field | Change |
|---|---|
| location | May now be an `rtsp` or `rtsps` address, with credentials |
| kind | `Stream` for any stream address, as for `http` today |
| display location | New, derived: the display address for a stream, the path for a file. What the row shows and what search matches |

## What is persisted

| Where | What | Form |
|---|---|---|
| Preferences | `rtspTransport` | `auto`, `tcp` or `udp` |
| `history.json`, recent files | An RTSP address, when recent files are on | Display address |
| `history.json`, positions | Nothing for RTSP (FR-006) | — |
| `session.m3u8` | RTSP entries, when session restore is on | Display address |
| A playlist file the user saves | Stream entries | Display address. Full address only when the user confirmed twice that passwords should be saved (FR-021) |
| Log | Any message that mentions an address | Display address |

Nothing else new is stored. No password is kept after GAV closes (FR-017).
