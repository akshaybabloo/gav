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
- The C++ tools run through `uvx` at the pinned version. If `uvx` is not installed, the command
  stops before doing anything and says where to get uv.
- A contributor who points `GAV_CLANG_FORMAT` or `GAV_CLANG_TIDY` at their own binary gets a
  version check instead: on a mismatch the command stops and prints the version found and the
  version needed.
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

Only [uv](https://docs.astral.sh/uv/). The commands fetch and run the pinned `clang-format` and
`clang-tidy` through `uvx`, with the versions from `support/lint-requirements.txt`; nothing else
is installed by hand. `qmlformat` and `qmllint` come with the Qt 6.12.0 installation the build
already uses.

To run a tool directly, for example from an editor:

```text
uvx --from clang-format==22.1.8 clang-format --version
uvx --from clang-tidy==22.1.8 clang-tidy --version
```

## Automatic checks

| Check | Runs on | When | Blocks merge |
|---|---|---|---|
| Check formatting | Linux x64 leg of the `build` job | Every pull request and every push to `main` | Yes |
| Lint | The same leg, after the build | The same | Yes |

- They build the same targets as the recipes above, with the same pinned tool versions. uv comes
  from the `astral-sh/setup-uv` action.
- "Check formatting" runs after CMake is configured and before the build; "Lint" runs after the
  build.
- A failing step's log ends with the command to run locally.
- They never apply fixes or push changes.
- Together they add no more than 5 minutes to the pull request (SC-005).

## Editor integration

The configuration files are at the repository root under their standard names, so an editor that
supports them formats and reports with the same rules without further setup. Whether a
contributor's editor has that support switched on is theirs to choose.
