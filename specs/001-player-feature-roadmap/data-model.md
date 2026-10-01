# Data Model: Player Feature Roadmap

**Feature**: [spec.md](./spec.md) | **Research**: [research.md](./research.md)

Entities are listed with the user story that introduces them. Types are conceptual. C++ types are
named where the choice matters.

## SubtitleTrack (Story 1)

One selectable subtitle source for the current media.

| Field | Type | Notes |
|-------|------|-------|
| id | string | `embedded:<streamIndex>` or `external:<absolutePath>`. Unique within the current media. |
| origin | enum `Embedded \| External` | |
| language | string (BCP 47 or ISO 639-2), optional | From stream tags or the sidecar filename suffix |
| title | string, optional | From stream tags. For external tracks, the filename |
| displayName | string | `title (language)`, then `language`, then `Track N` (FR-007) |
| codec | string | e.g. `ass`, `subrip`, `webvtt`, `mov_text` |
| header | string | ASS `[Script Info]` + `[V4+ Styles]` header from the probe, used to initialise the libass track |
| loadState | enum | See the state machine below |
| isDefault / isForced | bool | From stream disposition, used for automatic selection |

**Rules**

- Only text-based codecs are listed. Bitmap codecs (PGS, VobSub, DVB) are left out, as the spec's
  assumptions allow.
- There is at most one active track at a time. "Off" means no active track.

**State machine** (`loadState`)

```text
Pending ──probe header received──▶ Streaming ──probe "end" for stream──▶ Complete
   │                                   │
   └──────probe error / crash──────────┴──▶ Failed ──(embedded only)──▶ FallbackPlainText
```

When a track reaches `Failed`, GAV shows a snackbar. An external track that fails stays listed but
can't be chosen until it is reloaded.

## AudioTrack (Story 1)

| Field | Type | Notes |
|-------|------|-------|
| index | int | Qt's `audioTracks()` index, passed to `setActiveAudioTrack` |
| language | string, optional | `QMediaMetaData::Language` |
| title | string, optional | `QMediaMetaData::Title` |
| displayName | string | Same rule as SubtitleTrack |

## SubtitleSettings (Story 1, persisted)

| Field | Type | Default | Persistence |
|-------|------|---------|-------------|
| scale | real, 0.5–3.0 in 0.1 steps | 1.0 | QML `Settings` (FR-009) |
| preferredSubtitleLanguage | string | empty (none) | QML `Settings` (FR-010) |
| preferredAudioLanguage | string | empty (none) | QML `Settings` (FR-010) |

## SubtitleDelay (Story 1, per file, not persisted)

| Field | Type | Rules |
|-------|------|-------|
| delayMs | int | Changes in steps of ±100 ms (FR-008). Clamped to ±600 000 ms. Resets to 0 when a new file opens. Positive means subtitles appear later. |

## Chapter (Story 3)

| Field | Type | Notes |
|-------|------|-------|
| index | int | 0-based, in start-time order |
| startMs | int64 | |
| endMs | int64 | |
| title | string | From chapter metadata. Defaults to `Chapter N+1` |

**Rules**: Chapters are sorted by `startMs` with overlaps removed by the probe. Navigation follows
research R12.

## PlaybackHistoryEntry (Story 2, persisted in `history.json`)

| Field | Type | Notes |
|-------|------|-------|
| path | string | Absolute local path as opened. This is the identity key; different paths to the same file are separate entries, as the spec's edge cases accept. |
| positionMs | int64 | Last saved position |
| durationMs | int64 | Duration when saved, used to detect changed files |
| lastPlayed | ISO-8601 UTC datetime | Ordering for eviction and for recent files |

**Rules**

- An entry is written only when `0.05 × duration < position < 0.95 × duration` (FR-011).
  Positions in the last 5% delete the entry instead.
- The resume prompt is skipped and the entry discarded if `positionMs ≥` the file's current
  duration.
- Keep at most 200 entries, evicting the oldest `lastPlayed` first (FR-013).
- Network URLs never get entries. Resume is for local files only.

## RecentFile (Story 2, persisted in `history.json`)

| Field | Type | Notes |
|-------|------|-------|
| path | string | Local path. URLs are included too, so streams can be reopened |
| lastOpened | ISO-8601 UTC datetime | |

**Rules**: Up to 10 entries, newest first, without duplicates (reopening moves an entry to the
top). Choosing an entry whose file no longer exists shows "file not found" with a "Remove from
list" action.

`history.json` shape:

```json
{
  "version": 1,
  "positions": [{ "path": "...", "positionMs": 0, "durationMs": 0, "lastPlayed": "..." }],
  "recent": [{ "path": "...", "lastOpened": "..." }]
}
```

An unknown `version` or a parse failure is logged, the file is renamed to `history.json.bak`, and
GAV continues with empty history.

## PlaylistDocument (Story 2)

Used both for user-saved playlists and for the last-session playlist.

| Field | Type | Notes |
|-------|------|-------|
| entries | list of PlaylistEntry | In playlist order |
| currentIndex | int, optional | Session playlist only (`#GAV-CURRENT:<n>`) |

**PlaylistEntry**

| Field | Type | Notes |
|-------|------|-------|
| location | URL | `file://` for local paths, `http(s)://` for streams |
| title | string, optional | From `#EXTINF` |
| durationSec | int, optional | From `#EXTINF`. `-1` means unknown or live |

**Rules** (FR-019, research R8)

- On load, local entries that don't exist are skipped and counted. The UI reports the skipped
  count.
- Relative paths resolve against the playlist file's directory.
- If `currentIndex` points past the surviving entries, it is clamped. If nothing survives, it is
  cleared.

## ShuffleOrder (Story 2, in memory)

| Field | Type | Notes |
|-------|------|-------|
| enabled | bool | Persisted in QML `Settings` |
| remaining | list of int | Playlist indices not yet played this cycle |

**Rules** (research R9)

- Turning shuffle on keeps the current item and shuffles the others into `remaining`.
- Removing or reordering playlist items remaps or drops the affected indices.
- An empty `remaining` means the cycle is complete.

## ResumePrompt (Story 2, UI state)

```text
Closed ──open file with eligible entry──▶ Open (playback held at saved position, paused)
Open ──Resume / Enter──▶ Closed (play from saved position)
Open ──Start over / Esc──▶ Closed (seek 0, delete entry, play)
Open ──another file opened──▶ Closed (no change to entry)
```

## MediaSessionState (Story 4, published to the OS)

| Field | Source |
|-------|--------|
| playbackStatus | `Playing` / `Paused` / `Stopped`, from `CustomMediaPlayer.playbackState` |
| title / artist / album | `QMediaMetaData`. Title falls back to the filename or URL |
| durationMs / positionMs | Player. Duration is omitted for live streams |
| artworkUrl | Cached cover image (research R10), optional |
| canGoNext / canGoPrevious | Playlist position and shuffle state |
| canSeek | `QMediaPlayer::isSeekable()` |

Commands accepted from the OS: `Play`, `Pause`, `PlayPause`, `Stop`, `Next`, `Previous`,
`SetPosition`/`Seek`. These map to the same actions as the keyboard shortcuts.

## StreamSource (Story 4)

| Field | Type | Notes |
|-------|------|-------|
| url | URL | Must be `http` or `https`. Other schemes are rejected before loading |
| isLive | bool | `duration == 0 && !isSeekable` after `LoadedMedia` |
| displayTitle | string | Stream title metadata, falling back to the URL |
