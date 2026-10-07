# GAV

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/images/logo-bw.png" width="30%">
  <source media="(prefers-color-scheme: light)" srcset="assets/images/logo.png" width="30%">
  <img alt="GAV" src="assets/images/logo.png" width="30%">
</picture>

GAV is a simple audio and video player, backed by FFmpeg and Qt6.

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

### Subtitles

- Subtitle tracks inside the file are listed in the subtitle menu on the control bar, with full ASS/SSA styling.
- A subtitle file next to the video is picked up automatically when it has the same name: `movie.srt`, or `movie.en.srt` to mark the language. Supported files: srt, ass, ssa, vtt.
- Drag and drop a subtitle file onto the window, or use "Load subtitle file…" in the subtitle menu, to add one while playing.
- The same menu sets subtitle delay and size and selects the audio track. Preferred audio and subtitle languages are in Settings.

### Playlists

- Open and save playlists (`.m3u`, `.m3u8`) from the File menu, or drop one onto the window. Saving keeps each entry's title, duration, group and other `#EXTINF` attributes, in playlist order.
- With nothing playing, or audio only, the playlist fills the window. While a video plays, the playlist button on the control bar or `Ctrl+L` opens it as a panel over the right side of the picture. Drag the panel's left edge to resize it; click the picture or press `Esc` to close it.
- Each row shows the entry's title, its path or address, and whether it is a video, an audio file or a stream, with its duration (or "Live") and its group when the playlist gives one (`group-title`, `#EXTGRP`). Entries whose file is missing or that failed to play are marked "Unavailable" with the reason and are skipped by next and previous.
- `Ctrl+F` searches the playlist by title and group. The buttons next to the search field filter it to local files or streams, sort it by title or duration, and group it under the playlist's group headings, which can be collapsed. Next, previous and shuffle follow what is shown. None of this changes the order the playlist is saved in.
- Click selects a row (`Ctrl` and `Shift` select several) and double-click or `Enter` plays it. Right-click, or the `Menu` key, offers Play, Play next, Remove, and Show in file manager or Copy address. Entries queued with "Play next" play before normal order resumes.
- Drag a row by its handle to reorder, or drop files onto the list to insert them there. `Delete` removes the selection and `Ctrl+Z` undoes the last removal. "Remove duplicates" is in the playlist's "more" menu.
- The whole playlist works from the keyboard. Settings → Shortcuts lists the keys.
- "Show channel logos" (Settings → Playback, or the playlist's "more" menu) shows the image a playlist gives for an entry (`tvg-logo`). It is on by default; turn it off and nothing is downloaded. While it is on, logos are fetched only for the rows on screen, from `http` and `https` addresses, and kept in a 20 MB cache that "Clear history" empties.
- "Restore last playlist on startup" in Settings reopens the previous session's playlist. It is off by default, as are "Resume where I left off" and "Remember recent files".

### Streams

- File > Open URL (`Ctrl+N`) plays an `http://` or `https://` address: a video or audio file, an HLS stream (`.m3u8`) or a remote playlist (`.m3u`).
- For HLS streams that offer several qualities, the quality button on the control bar lists every video quality and audio format. Auto picks one from a short download-speed check.
- On-demand HLS streams list their subtitle tracks in the subtitle menu.

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

> [!NOTE]
> Requires CMake 4.0 or higher.

## Requirements

- Qt6 with QtMultimedia module
- CMake 4.0 or higher
- vcpkg
- Ninja (optional, but recommended)
- Visual Studio 2022 or higher / GCC 10 or higher / Clang 10 or higher (depending on your platform)

### Building with vcpkg and Ninja

Intsall your vcpk in user folder and run the following code:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=$QT/lib/cmake/Qt6/qt.toolchain.cmake -S .
cd build
ninja
```

Add `-DCMAKE_BUILD_TYPE=Release` to the cmake command for a release build.

This should install any required dependencies automatically and build the project.

### Formatting and linting

The style and the lint rules are in `.clang-format`, `.clang-tidy`, `.qmlformat.ini` and `.qmllint.ini`. The commands are [just](https://just.systems/) recipes and use the `build-cli/` directory:

| Command | What it does |
|---|---|
| `just format` | Formats the C++ and QML sources in place |
| `just format-check` | Lists the files `just format` would change, without changing them |
| `just lint` | Builds the project, then runs clang-tidy and qmllint and reports each finding |

`just format-cpp`, `just format-qml`, `just lint-cpp` and `just lint-qml` do the same for one language.

The commands need clang-format and clang-tidy 23.1.0 on `PATH`, for example from the [LLVM release](https://github.com/llvm/llvm-project/releases). Any other version is refused with a message. To use binaries that are not on `PATH`, configure with `-DGAV_CLANG_FORMAT=<path>` and `-DGAV_CLANG_TIDY=<path>`. qmlformat and qmllint come with Qt.
