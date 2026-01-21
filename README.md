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
gav <file>
```

Or open a file from the menu.

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
