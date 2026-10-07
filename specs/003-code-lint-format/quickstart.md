# Quickstart: Validating Code Linting and Formatting

How to check that the feature works. Commands are in
[contracts/commands.md](./contracts/commands.md) and the rules in
[contracts/rules.md](./contracts/rules.md).

## Prerequisites

- A checkout that builds with `just build`.
- [uv](https://docs.astral.sh/uv/) installed (`uvx --version` works). The commands fetch the
  pinned `clang-format` and `clang-tidy` themselves.

- `just test` passing before you start, so that later failures are attributable.

## Automated checks

| Command | Expected |
|---|---|
| `just format-check` | Prints the tool versions, reports no files, exits 0 (SC-001) |
| `just lint` | Prints the tool versions, reports no findings, exits 0 (SC-001) |
| `just test` | Same result as before the feature: every test passes (SC-004) |
| `LC_ALL=C LANG=C just test` | The same under the `C` locale, which is what CI uses |

## Story 1: format a change with one command

| # | Steps | Expected |
|---|-------|----------|
| Q1.1 | On a clean checkout, run `just format-check` | No files reported, exit 0 (FR-005) |
| Q1.2 | Run `just format`, then `git status` | Nothing changed |
| Q1.3 | Re-indent a function in `playlistmodel.cpp` by hand, and put two attributes on one line in `PlaylistRow.qml`. Run `just format-check` | Both files are named, nothing is modified, exit is non-zero, and the output says to run `just format` (FR-004) |
| Q1.4 | Run `just format`, then `git diff` | No difference from the committed files (FR-003) |
| Q1.5 | `grep -c 'u[0-9a-f]\{4\}' *.qml` before and after `just format` | The same counts: no icon escape was rewritten (FR-007) |
| Q1.6 | Wrap a hand-aligned array in the opt-out markers from the rules contract, with a reason, and run `just format` | The region keeps its layout (FR-008) |
| Q1.7 | Configure with `GAV_CLANG_FORMAT` pointing at a `clang-format` of a different version and run `just format-check`. Then hide `uvx` from `PATH` with no override and run it again | The first stops before checking anything and prints the version found and the version needed. The second stops and says where to get uv (FR-022) |
| Q1.8 | `git blame` a line that the reformat touched, with `blame.ignoreRevsFile` set | The line is attributed to the change before the reformat (FR-009) |
| Q1.9 | Time `just format` | Under 30 s (SC-002) |

## Story 2: catch likely defects before review

| # | Steps | Expected |
|---|-------|----------|
| Q2.1 | On a clean, built checkout, run `just lint` | No findings, exit 0 (FR-013) |
| Q2.2 | On a checkout that has never been built, run `just lint` | It builds first, then lints; it does not fail with missing-file errors |
| Q2.3 | Introduce the five lint problems from the table below, run `just lint` | All five are reported with file, line and rule; exit is non-zero (FR-012) |
| Q2.4 | Suppress one of them with the form from the rules contract, including a reason | That finding is gone; the others remain (FR-014) |
| Q2.5 | Remove the reason from the suppression | `just lint` rejects the suppression itself (SC-007) |
| Q2.6 | Replace the check name with a bare `NOLINT` | Rejected in the same way |
| Q2.7 | Edit only `mediasession_windows.cpp` and run `just lint` | It is not analysed. The list of exempt files in the contributor documentation and in `support/lint.cmake` says why (FR-016) |
| Q2.8 | Time `just lint` on a built tree | Under 5 minutes (SC-002) |
| Q2.9 | Run the application headless after the QML lint fixes and open each view that was touched | No QML warnings in the log, and the view behaves as before |

## Story 3: every change is checked automatically

| # | Steps | Expected |
|---|-------|----------|
| Q3.1 | Open a pull request with no violations | "Check formatting" and "Lint" pass |
| Q3.2 | Push a commit with one misformatted C++ file | "Check formatting" fails before the build starts, names the file and says to run `just format` (FR-020) |
| Q3.3 | Push a commit with one lint finding | "Lint" fails and shows file, line, rule and message |
| Q3.4 | Compare the Linux x64 leg's duration with a run from before the feature | No more than 5 minutes longer (SC-005) |
| Q3.5 | Read the start of each step's log | The tool versions are printed and match `support/lint-requirements.txt` (FR-021) |

## Seeded violations (SC-003)

Introduce all ten on a throwaway branch, push it as a pull request, and confirm the checks report
each one. Then delete the branch.

| # | Kind | Where | What |
|---|---|---|---|
| 1 | Format | A `.cpp` file | A function body indented with two spaces |
| 2 | Format | A `.h` file | `QObject* parent` instead of `QObject *parent` |
| 3 | Format | A test file | A statement split across three lines that fits in one |
| 4 | Format | A `.qml` file | Two attributes on one line |
| 5 | Format | A `.qml` file | A block indented with two spaces |
| 6 | Lint | A `.cpp` file | An `if` without braces |
| 7 | Lint | A `.cpp` file | A variable read before it is given a value |
| 8 | Lint | A `.cpp` file | A range-for that copies each element where a reference would do |
| 9 | Lint | A `.qml` file | A property of the root object used without `root.` |
| 10 | Lint | A `.qml` file | An import that nothing uses |

**Expected**: ten of ten reported, each with its file, and the five lint ones with line and rule.

## What cannot be checked on one machine

- That the reformatted code compiles and passes on Windows and macOS: CI on the pull request.
- That formatting is byte-identical on another platform (SC-006): run `just format-check` on a
  Windows or macOS checkout.
