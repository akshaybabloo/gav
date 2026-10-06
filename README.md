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

- Open and save playlists (`.m3u`, `.m3u8`) from the File menu, or drop one onto the window.
- Each row shows the entry's path or address and says whether it is a video, an audio file or a stream, with its duration (or "Live") and its group when the playlist gives one (`group-title`, `#EXTGRP`). Entries whose file is missing or that failed to play are marked "Unavailable" with the reason and are skipped by next and previous.
- While a video plays, the playlist button on the control bar or `Ctrl+L` opens the playlist as a panel over the right side of the picture. Drag its left edge to resize it; click the picture or press `Esc` to close it.
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
