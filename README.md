<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/images/logo-bw.png" width="10%">
  <source media="(prefers-color-scheme: light)" srcset="assets/images/logo.png" width="10%">
  <img alt="GAV" src="assets/images/logo.png" width="10%">
</picture>

GAV is a simple audio and video player, backed by FFmpeg and Qt6.

## Installation

Download GAV for your platform from the [latest release](https://github.com/akshaybabloo/gav/releases/latest):

- Windows (x64): installer (`.exe`) or `.zip`
- macOS (Apple silicon): `.dmg` or `.tar.gz`
- Linux (x86_64 and aarch64): `.AppImage`, `.deb`, `.rpm` or `.tar.gz`

On Linux, GAV also needs the xcb cursor library:

For Debian-based distributions:

```bash
sudo apt update && sudo apt install -y libxcb-cursor0
```

For Red Hat-based distributions:

```bash
sudo dnf install xcb-util-cursor
```

For Arch-based distributions:

```bash
sudo pacman -S xcb-util-cursor
```

## Usage

From command line:

```bash
# Play a video or audio file
gav <file>

# Open several files, or a playlist
gav <file1> <file2> ...
gav <playlist.m3u8>

# Play a web address: a media file, an HLS stream or a remote playlist
gav https://example.org/stream.m3u8

# Create a collage from video files
gav --collage <file1> <file2> ...

# Create collages from all videos in a folder
gav --collage <folder>
```

Or open a file from the menu, or drag and drop files onto the window.

## Features

### Subtitles

- Subtitle tracks inside the file are listed in the subtitle menu on the control bar, with full ASS/SSA styling.
- A subtitle file next to the video is picked up automatically when it has the same name: `movie.srt`, or `movie.en.srt` to mark the language. Supported files: srt, ass, ssa, vtt.
- Drag and drop a subtitle file onto the window, or use "Load subtitle file…" in the subtitle menu, to add one while playing.
- The same menu sets subtitle delay and size and selects the audio track. Preferred audio and subtitle languages are in Settings.

### Playlists

- Open and save playlists (`.m3u`, `.m3u8`) from the File menu, or drop one onto the window. Saving keeps each entry's title, duration, group and other `#EXTINF` attributes, in playlist order.
- With nothing playing, or audio only, the playlist fills the window. While a video plays, the playlist button on the control bar or `Ctrl+L` opens it as a panel over the right side of the picture. Each newly opened file or stream starts with the panel closed; it stays open while you move between entries. Drag the panel's left edge to resize it; click the picture or press `Esc` to close it.
- Each row shows the entry's title, its path or address, and whether it is a video, an audio file or a stream, with its duration (or "Live") and its group when the playlist gives one (`group-title`, `#EXTGRP`). Entries whose file is missing or that failed to play are marked "Unavailable" with the reason and are skipped by next and previous.
- `Ctrl+F` searches the playlist by title and group. The buttons next to the search field filter it to local files or streams, sort it by title or duration, and group it under the playlist's group headings, which can be collapsed. Next, previous and shuffle follow what is shown. None of this changes the order the playlist is saved in.
- Click selects a row (`Ctrl` and `Shift` select several) and double-click or `Enter` plays it. Right-click, or the `Menu` key, offers Play, Play next, Remove, and Show in file manager or Copy address. Entries queued with "Play next" play before normal order resumes.
- Drag a row by its handle to reorder, or drop files onto the list to insert them there. `Delete` removes the selection and `Ctrl+Z` undoes the last removal. "Remove duplicates" is in the playlist's "more" menu.
- The whole playlist works from the keyboard. Settings → Shortcuts lists the keys.
- "Show channel logos" (Settings → Playback, or the playlist's "more" menu) shows the image a playlist gives for an entry (`tvg-logo`). It is on by default; turn it off and nothing is downloaded. While it is on, logos are fetched only for the rows on screen, from `http` and `https` addresses, and kept in a 20 MB cache that "Clear history" empties.
- "Restore last playlist on startup" in Settings reopens the previous session's playlist. It is off by default, as are "Resume where I left off" and "Remember recent files".

### Split view

- Select two to four entries in the playlist, right-click and choose "Play in split view" to play them side by side: two next to each other, three or four in a 2×2 grid. With one video already playing, "Play in split view" on another entry puts it beside the one playing.
- One video is selected, marked with a border in the accent colour. Only it is heard, and the control bar, the keyboard and the media keys act on it. Click a video, or press `1`–`4`, to select another. Each video keeps its own position, speed, tracks, subtitles, repeat and picture settings.
- Double-click a video to fill the window with it (the other files pause; cameras and other live streams keep playing); double-click again for full screen, and once more to go back to the grid. `Esc` leaves full screen, then goes back to the grid. `1`–`4` switch the video filling the window.
- Opening a file or address, or playing a playlist entry, replaces the selected video. Drop a file onto a video to replace it, or onto an empty cell to add it. Next and previous change the selected video and skip entries already showing.
- Stop closes the selected video; View > End split view keeps only the selected one. A video that ends or fails keeps its place until it is closed or replaced.
- Several RTSP cameras open one after another, so four cameras on a local network take a few seconds to all start. The mini player and the resume prompt are not available in a split view, and a split view is not restored at the next start.

### Streams

- File > Open URL (`Ctrl+N`) plays an `http://` or `https://` address: a video or audio file, an HLS stream (`.m3u8`) or a remote playlist (`.m3u`).
- It also plays `rtsp://` and `rtsps://` addresses, such as IP cameras, as live streams. They can be given on the command line and in playlists too.
- Settings > Playback > RTSP transport chooses how an RTSP stream is carried. Automatic tries UDP first and switches to TCP when nothing arrives; TCP and UDP use that one only. Stats for nerds shows the transport in use.
- An RTSP stream that drops is reconnected up to three times over about 30 seconds.
- Stopping an RTSP stream closes GAV's connection at once. A source that carries the stream over UDP may keep its side open until its own timeout, usually about a minute, so a camera that limits viewers can refuse a new connection during that time.
- For HLS streams that offer several qualities, the quality button on the control bar lists every video quality and audio format. Auto picks one from a short download-speed check.
- On-demand HLS streams list their subtitle tracks in the subtitle menu.
- A user name and password inside an address (`rtsp://user:password@…`) are used to sign in and nowhere else: GAV leaves them out of what it shows, logs and remembers. Saving a playlist writes them only after you confirm twice. An address given on the command line is still visible to other programs in the system's process list.

### Keyboard shortcuts

The full list is in Settings under the Shortcuts tab.

### Collage Creation

GAV can create thumbnail collages from video files. A collage is a grid of video frames extracted at evenly distributed timestamps, along with metadata (filename, duration, resolution, codecs, file size).

**CLI Usage:**
- Process individual files: `gav --collage video1.mp4 video2.mp4`
- Process all videos in a folder: `gav --collage /path/to/video/folder`
- Mix files and folders: `gav --collage video1.mp4 /path/to/folder video2.mkv`

Supported video formats: mp4, avi, mkv, mov, wmv, flv, webm, m4v, mpg

## Build

### Requirements

- Qt 6.12 with the QtMultimedia module
- CMake 4.0 or higher
- vcpkg
- Ninja
- [just](https://just.systems/) (optional, runs the commands below)
- Visual Studio 2022 or higher / GCC 10 or higher / Clang 10 or higher (depending on your platform)

### Building

Install vcpkg in your home folder (`~/vcpkg`) and Qt in `~/Qt/6.12.0/<compiler>`, then run:

| Command | What it does |
|---|---|
| `just build` | Configures `build-cli/` on first use and builds GAV |
| `just run <args>` | Builds and runs GAV, passing the arguments on, e.g. `just run video.mkv` |
| `just test` | Builds and runs the unit tests |
| `just clean` | Removes the build directory |

vcpkg installs the other dependencies during the first configure. The environment variables `VCPKG_ROOT`, `QT_ROOT`, `GAV_BUILD_DIR` and `GAV_BUILD_TYPE` (default `Debug`) override the defaults. Run `just configure` again after changing them.

Without just, configure and build with CMake directly:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_PREFIX_PATH=$HOME/Qt/6.12.0/gcc_64
cmake --build build --target appgav
```

Replace `gcc_64` with `macos` or `msvc2022_64` on macOS and Windows.

### Formatting and linting

The style and the lint rules are in `.clang-format`, `.clang-tidy`, `.qmlformat.ini` and `.qmllint.ini`. The commands are just recipes and use the `build-cli/` directory:

| Command | What it does |
|---|---|
| `just format` | Formats the C++ and QML sources in place |
| `just format-check` | Lists the files `just format` would change, without changing them |
| `just lint` | Builds the project, runs clang-tidy and qmllint and reports each finding, then checks the lint suppression markers |

`just format-cpp`, `just format-qml`, `just lint-cpp` and `just lint-qml` do the same for one language.

The commands need clang-format and clang-tidy 23.1.0 on `PATH`, for example from the [LLVM release](https://github.com/llvm/llvm-project/releases). Any other version is refused with a message. To use binaries that are not on `PATH`, configure with `-DGAV_CLANG_FORMAT=<path>` and `-DGAV_CLANG_TIDY=<path>`. qmlformat and qmllint come with Qt.
