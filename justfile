# use PowerShell instead of sh:
set windows-shell := ["pwsh.exe", "-c"]

build_dir := env("GAV_BUILD_DIR", "build-cli")
build_type := env("GAV_BUILD_TYPE", "Debug")
vcpkg_root := env("VCPKG_ROOT", home_directory() / "vcpkg")
qt_root := env("QT_ROOT", home_directory() / "Qt" / "6.12.0" / if os() == "windows" { "msvc2022_64" } else if os() == "macos" { "macos" } else { "gcc_64" })
exe := if os() == "windows" { ".exe" } else { "" }
configure_if_needed := if path_exists(build_dir / "CMakeCache.txt") == "true" { "cmake -E true" } else { "just configure" }

default: (help)

# Print this help message
@help:
    echo "run 'just list' to list targets"
    echo "more information can be found at  at http://just.systems/"
    just list

# List the recipes and descriptions
list:
    @just --list

# Configure the build directory (GAV_BUILD_DIR, GAV_BUILD_TYPE, VCPKG_ROOT and QT_ROOT override the defaults)
configure:
    cmake -S . -B {{ build_dir }} -G Ninja -DCMAKE_BUILD_TYPE={{ build_type }} -DCMAKE_TOOLCHAIN_FILE={{ vcpkg_root / "scripts" / "buildsystems" / "vcpkg.cmake" }} -DCMAKE_PREFIX_PATH={{ qt_root }}

# Build the gav application
build:
    @{{ configure_if_needed }}
    cmake --build {{ build_dir }} --target appgav

# Build and run the unit tests; extra arguments go to ctest, e.g. `just test -R ShuffleOrder`
test *args:
    @{{ configure_if_needed }}
    cmake --build {{ build_dir }} --target gav_tests
    ctest --test-dir {{ build_dir }} --output-on-failure {{ args }}

# Build and run gav; extra arguments go to gav, e.g. `just run --verbose video.mkv`
run *args: build
    {{ build_dir / "gav" + exe }} {{ args }}

# Remove the build directory
clean:
    cmake -E rm -rf {{ build_dir }}
