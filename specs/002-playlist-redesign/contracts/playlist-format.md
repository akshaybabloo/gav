# Contract: Playlist File Format (extended)

Extends [the existing playlist format](../../001-player-feature-roadmap/contracts/playlist-format.md).
Everything there still holds unless this document says otherwise. Used by FR-006, FR-012, FR-026,
FR-027 and SC-008.

## Writing

```text
#EXTM3U
#EXTINF:5400,The Movie
/home/user/Videos/The Movie.mkv
#EXTINF:-1 tvg-id="News.uk" tvg-logo="https://example.org/news.png" group-title="News",News Channel
https://example.org/news/index.m3u8
```

- The `#EXTINF` line is `#EXTINF:<seconds>[ <attributes>],<title>`.
- `<attributes>` is the entry's stored attribute text, written back unchanged, with one exception:
  when the entry has a group, `group-title="<group>"` is present and matches it (added if missing,
  replaced if different).
- A double quote inside a group name is written as `'`, because the attribute syntax has no
  escape.
- Every entry is written, in playlist order, whatever search, filter, grouping or sort is active.
- Unavailable entries are written like any other.
- `#GAV-CURRENT:<int>` is still written for the session playlist only.

## Reading

Additions to the existing line handling:

| Line | Handling |
|------|----------|
| `#EXTINF:<number>[ <attributes>],<title>` | The title is the text after the first comma that is outside double quotes. `<attributes>` is kept verbatim. `group-title` and `tvg-logo` are read from it. |
| `#EXTGRP:<name>` | Sets the group of the next location line, unless its `#EXTINF` has a `group-title`. |

Changed rule:

- A local entry that does not exist is **kept** and marked unavailable with the reason "File not
  found". It was previously skipped. The result's `skippedMissing` count is replaced by
  `unavailable`.

Unchanged: entries with an unsupported extension or scheme are skipped and counted; a remote
playlist cannot contribute local-file entries; a remote document must carry `#EXTM3U` or
`#EXTINF` to be accepted.

Attribute rules:

- Attribute names are matched case-insensitively. Values are double-quoted; an unquoted value ends
  at the next space.
- `tvg-logo` is kept only when it is an `http` or `https` address. Other values are ignored for
  display but stay in the attribute text.
- Attributes GAV does not use (`tvg-id`, `tvg-name`, `http-user-agent`, …) are neither interpreted
  nor dropped.

## Result reported to the UI

Each entry gains `group`, `logo`, `attributes`, `available` and `reason`, next to the existing
`path`, `title` and `durationSec`. The result object gains `unavailable` and loses
`skippedMissing`.

## Round trip (SC-008)

Reading a playlist and writing it again produces the same entries in the same order with the same
titles, durations, groups and attribute text. Byte-for-byte equality is not promised: line endings
are normalised to `\n`, local paths are written absolute, and comment lines other than
`#EXTINF`, `#EXTGRP` (folded into `group-title`) and `#GAV-CURRENT` are not preserved.
