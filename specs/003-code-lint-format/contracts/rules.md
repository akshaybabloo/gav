# Contract: Styles, Rule Sets, Suppressions and Exemptions

Covers FR-001, FR-002, FR-007, FR-008, FR-010, FR-011, FR-014, FR-016 and SC-007. The reasons for
each choice are in [research.md](../research.md).

## C++ style (`.clang-format`)

| Setting | Value |
|---|---|
| Base style | LLVM |
| Indent | 4 spaces |
| Column limit | 160 |
| Access modifiers | At the class's indent level |
| Pointers and references | Next to the name (`QObject *parent`) |
| Include order | As written; not sorted |
| Braces | Not added or removed by the formatter |
| Line endings | Unix |

## QML style (`.qmlformat.ini`)

| Setting | Value |
|---|---|
| Indent | 4 spaces |
| Column limit | None |
| Attribute order | As written; not normalised |
| Line endings | Unix |

String contents are never changed by either formatter. In particular the `\uXXXX` icon escapes in
QML stay as escapes.

## C++ rule set (`.clang-tidy`)

- Warnings are errors.
- Only first-party headers are reported.
- On in full: `clang-analyzer-*`, `bugprone-*`, `performance-*`, `portability-*`.
- Off within those groups, each with its reason in the file:
  `bugprone-easily-swappable-parameters`, `bugprone-throwing-static-initialization`,
  `performance-enum-size`.
- On by name: `cert-err33-c` and the selected `modernize-`, `readability-`, `misc-` and
  `cppcoreguidelines-` checks listed, each by its full name, in research R4. `.clang-tidy` is the
  authoritative list.
- Everything else is off. Adding a check later is a change to this file plus whatever fixes it
  needs, in one pull request.

## QML rule set (`.qmllint.ini`)

- Every category at qmllint's default severity, except unused imports, which are raised from
  information to warning so that they fail the check.
- The warning limit is zero: any warning fails the check.

## Suppressing one finding

| Language | Form |
|---|---|
| C++ | `// NOLINT(check-name): reason` at the end of the line, or `// NOLINTNEXTLINE(check-name): reason` on the line above |
| QML | A comment line giving the reason, then `// qmllint disable category`, the code, then `// qmllint enable category` |

**Rules**

- A suppression names exactly the check or category it silences. A bare `NOLINT` or a bare
  `qmllint disable` is rejected by `just lint`.
- A suppression without a reason is rejected by `just lint`.
- Suppress only when the finding is a false positive or the code is intentional. Otherwise fix it.

## Keeping a hand-made layout

| Language | Form |
|---|---|
| C++ | `// clang-format off: reason`, the code, `// clang-format on` |
| QML | A comment line giving the reason, then `// qmlformat off`, the code, `// qmlformat on` |

An opt-out without a reason is rejected by `just lint`.

## Files exempt from the C++ linter

| File | Reason |
|---|---|
| `mediasession_windows.cpp` | Builds on Windows only; lint runs on Linux |
| `mediasession_macos.mm` | Builds on macOS only; lint runs on Linux |

Both are still formatted. No QML file is exempt.

## Reformatting changes

`.git-blame-ignore-revs` lists the commits that only reformat, one full commit id per line with a
comment above saying what was reformatted. To have `git blame` skip them locally:

```text
git config blame.ignoreRevsFile .git-blame-ignore-revs
```
