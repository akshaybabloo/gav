# Contract: Keyboard Shortcuts and Command Line

## Keyboard shortcuts (FR-024–FR-026)

This table is the source of truth for the shortcuts reference in `SettingsDialog.qml` (FR-025).

**Scope**

- **Global**: works whenever the main window has focus, including in the mini player.
- **Text-safe**: still works while a text field has focus.
- **Video**: needs video. **Paused**: needs paused video.
- **Chapters**: needs media with chapters. **Playlist**: needs more than one item.
- **Subtitles**: needs a subtitle track. **Audio tracks**: needs more than one audio track.

| Key | Action | Scope | Status |
|-----|--------|-------|--------|
| Space | Play / pause | Global | Existing |
| Left / Right | Seek −5 s / +5 s | Global | Existing |
| Scroll / Ctrl+Scroll | Volume / zoom | Global | Existing |
| I | Stats overlay | Video | Existing |
| Double-click | Full-screen | Video | Existing |
| Ctrl+Up / Ctrl+Down | Volume +5% / −5% | Global | New |
| M | Mute toggle | Global | New |
| F | Full-screen toggle | Video | New |
| Esc | Exit full-screen | Full-screen, no dialog open | New |
| `[` / `]` / `=` | Previous / next speed preset, reset to 1.0× | Global | New |
| E / Shift+E | Next frame / previous frame | Video, pauses first | New |
| Shift+N / Shift+P | Next / previous chapter | Chapters | New |
| N / P | Next / previous playlist item | Playlist | New |
| Ctrl+T | Go to time… | Global, text-safe | New |
| V | Cycle subtitle track (…→ Off →…) | Subtitles | New |
| B | Cycle audio track | Audio tracks | New |
| G / H | Subtitle delay −100 ms / +100 ms | Subtitles | New |
| Ctrl+O | Open file… | Global, text-safe | New |
| Ctrl+N | Open URL | Global, text-safe | New |

**Rules**

- Shortcuts that don't apply in the current state do nothing and show no error.
- Every action that changes a value shows a short on-screen indicator, reusing the existing
  overlay style: volume, speed, subtitle delay (`Subtitle delay: +300 ms`), and the active
  subtitle or audio track.
- In the resume prompt: Enter means Resume and Esc means Start over (FR-012).

## Command line

These additions keep existing behaviour unchanged.

```text
gav [files-or-urls...]
gav --source <file-or-url>
gav --collage <files-or-folders...>        # unchanged; URLs rejected
```

- Positional arguments and `--source` now accept `http://` and `https://` URLs (FR-029). Other
  schemes are rejected with a message on stderr and exit code 2 if nothing playable is left.
- Arguments are opened in order. With "Restore last playlist" on, the restored playlist loads
  first and the arguments are appended, with the first one played (spec edge case).
- Single-instance hand-off (`InstanceManager`) forwards URLs the same way it forwards files.
- `--probe` and `--subtitle-file` exist only for internal subprocess use with `GAV_SUBPROCESS=probe`
  (see [probe-protocol.md](./probe-protocol.md)) and are hidden from `--help`.
