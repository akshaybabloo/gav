# Contract: Commands and Automatic Checks

What a contributor and the maintainer can rely on. Covers FR-003, FR-004, FR-012, FR-019 to
FR-022 and SC-002.

## Local commands

All three are `just` recipes and use the `build-cli/` directory, like `just build` and `just test`.

| Command | Changes files | Succeeds when | On failure it prints |
|---|---|---|---|
| `just format` | Yes: every first-party C++ and QML file that does not match its style | Always, unless a tool is missing or has the wrong version | The tool problem |
| `just format-check` | No | Every first-party C++ and QML file matches its style | Each file that would change, then "run `just format`" |
| `just lint` | No | There are no findings and every suppression has a rule and a reason | Each finding as `file:line: message [rule]`, then "run `just lint`" for reproducing |

**Common behaviour**

- Each command starts by printing the versions of the tools it uses.
- clang-format and clang-tidy are the ones on `PATH`. Each one's version is checked before
  anything else happens: on a mismatch the command stops and prints the version found and the
  version needed, and if the tool is missing it says which version is needed.
- A contributor who points `GAV_CLANG_FORMAT` or `GAV_CLANG_TIDY` at a binary when configuring
  gets that binary, with the same version check.
- "First-party" means the sources of the application and the unit tests and the QML files of the
  module, as the build defines them. Generated code, build output, downloaded dependencies and
  `vcpkg-ports/` are never read or written.
- `just lint` builds the application and the tests first if they are not built, because both
  linters need files the build generates.
- `just lint` only reports. It never changes a file.
- Exit status is zero on success and non-zero otherwise, so the commands can be chained.

**Parts of `just lint`** (also available on their own)

| Recipe | Runs |
|---|---|
| `just lint-cpp` | clang-tidy over each first-party C++ source, in parallel |
| `just lint-qml` | qmllint over the QML module |

**Parts of `just format`**

| Recipe | Runs |
|---|---|
| `just format-cpp` | clang-format over the first-party C++ files |
| `just format-qml` | qmlformat over the QML files |

None of these runs as part of `just build`, `just test` or packaging.

## What a contributor installs

`clang-format` and `clang-tidy` 23.1.0, on `PATH`. The version is fixed in `support/lint.cmake`
(`GAV_LLVM_VERSION`), and the commands fetch nothing themselves. The maintainer plans to publish
builds of the two tools; until then they are in the
[LLVM 23.1.0 release](https://github.com/llvm/llvm-project/releases). `qmlformat` and `qmllint`
come with the Qt 6.12.0 installation the build already uses.

To use binaries that are not on `PATH`, name them when configuring:

```text
-DGAV_CLANG_FORMAT=<path> -DGAV_CLANG_TIDY=<path>
```

## Automatic checks

| Check | Runs on | When | Blocks merge |
|---|---|---|---|
| Check formatting | Linux x64 leg of the `build` job | Every pull request and every push to `main` | Yes |
| Lint | The same leg, after the build | The same | Yes |

- They build the same targets as the recipes above, with the same pinned tool versions. The
  runner gets clang-format and clang-tidy from the builds the maintainer publishes.
- "Check formatting" runs after CMake is configured and before the build; "Lint" runs after the
  build.
- A failing step's log ends with the command to run locally.
- They never apply fixes or push changes.
- Together they add no more than 5 minutes to the pull request (SC-005).

## Editor integration

The configuration files are at the repository root under their standard names, so an editor that
supports them formats and reports with the same rules without further setup. Whether a
contributor's editor has that support switched on is theirs to choose.
