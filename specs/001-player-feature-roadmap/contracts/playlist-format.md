# Contract: Playlist File Format

Used by FR-018/FR-019 (playlists the user saves and opens) and FR-019a (the last-session
playlist). See data model: [PlaylistDocument](../data-model.md#playlistdocument-story-2).

## Writing

- Encoding is UTF-8 without a BOM, with `\n` line endings.
- The default extension is `.m3u8`. The save dialog also offers `.m3u`, which uses the same
  content.

```text
#EXTM3U
#EXTINF:5400,The Movie
/home/user/Videos/The Movie.mkv
#EXTINF:-1,Radio Stream
https://example.org/live.m3u8
```

- Local entries are written as absolute native paths.
- `#EXTINF` duration is whole seconds, or `-1` if unknown or live.
- The title is the item's display name.

**Session playlist only**: one extra line after `#EXTM3U`:

```text
#GAV-CURRENT:2
```

Other players treat it as a comment, so the session file is still a valid M3U.

## Reading

Readers accept:

- `.m3u` and `.m3u8`, with or without `#EXTM3U`
- an optional UTF-8 BOM
- `\n` or `\r\n` line endings

Line handling:

| Line | Handling |
|------|----------|
| Empty, or a `#` comment other than those below | Ignored |
| `#EXTINF:<int>,<title>` | Applies to the next location line |
| `#GAV-CURRENT:<int>` | Sets `currentIndex` (session restore only) |
| `file://…` | Converted to a local path |
| `http://…`, `https://…` | Kept as a stream URL |
| Any other scheme (`ftp:`, `rtsp:`, …) | Skipped and counted as unsupported |
| Relative path | Resolved against the playlist file's directory |
| Absolute path | Used as is |

Other rules:

- Local entries that don't exist are kept and marked unavailable with the reason "File not found".
  They were skipped until the playlist redesign; see
  [the extended contract](../../002-playlist-redesign/contracts/playlist-format.md), which also adds
  `group-title`, `tvg-logo` and `#EXTGRP`.
- Local entries whose extension isn't a supported audio/video type are skipped and counted as
  unsupported.
- `.m3u` files that aren't valid UTF-8 are decoded as the system's local 8-bit encoding.

## Result reported to the UI

```text
{ entries: [...], currentIndex: int|null, unavailable: int, skippedUnsupported: int }
```

When either count is above zero, the UI shows "Loaded N items, M unavailable", "Loaded N items,
skipped M unsupported", or both joined.
