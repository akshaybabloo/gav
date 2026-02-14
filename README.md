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

# Create a collage from video files
gav --collage <file1> <file2> ...

# Create collages from all videos in a folder
gav --collage <folder>
```

Or open a file from the menu.

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
