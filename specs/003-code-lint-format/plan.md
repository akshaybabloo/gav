# Implementation Plan: Code Linting and Formatting

**Branch**: `003-code-lint-format` (spec directory; work branches are cut from GitHub issues, starting with `195-code-lint-format`) | **Date**: 2026-10-07 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `specs/003-code-lint-format/spec.md`

## Summary

GAV gets one formatting style and one lint rule set for each of its two languages, the existing
code is brought in line with them, and every pull request is checked against them.

1. **Format with one command**: `just format` rewrites the C++ and QML sources in the project
   style, and `just format-check` reports what would change.
2. **Lint with one command**: `just lint` runs clang-tidy over the C++ sources and qmllint over
   the QML module and lists each finding with its file, line and rule.
3. **Automatic checks**: both run on every pull request and on `main`, and a failure blocks the
   merge.

**Technical approach** (from [research.md](./research.md))

- Tools: `clang-format` and `clang-tidy` 22.1.8 from PyPI wheels, pinned in one requirements file
  and run through `uvx` so the pinned version is the only one that can run; `qmlformat` and
  `qmllint` from the Qt 6.12.0 the project already pins. Nothing is added to the application.
- Styles are chosen by measuring which candidate changes the fewest lines: LLVM-based with indent
  4 and a 160-column limit for C++ (11.5 % of lines, three quarters of them in the two files that
  use a different indent today), qmlformat defaults for QML (3.6 %).
- The C++ rule set is an explicit selection (four whole groups of defect checks plus about fifty
  named ones) that finds defects rather than expresses taste: 159 findings to resolve, against
  3,090 with everything on. The QML rule set is qmllint's
  defaults with warnings made fatal: 388 findings, 362 of them unqualified names.
- The commands are CMake targets in a new `support/lint.cmake`, wrapped by `justfile` recipes.
  The file lists come from the build's own targets, so generated and vendored code is never
  touched and the commands behave the same on all three platforms.
- The automatic checks are two steps in the existing Linux x64 CI leg, which already has
  everything they need.
- Delivery is in six pull requests so each one is reviewable: tooling with no source changes,
  the reformat, the C++ lint fixes, the QML lint fixes, turning the checks on in CI, then
  documentation and the constitution amendment.

## Technical Context

**Language/Version**: C++20 and QML (Qt 6.12). CMake 4 for the lint targets.

**Primary Dependencies**: Development only: `clang-format` 22.1.8 and `clang-tidy` 22.1.8 (PyPI wheels, run with `uvx`), `uv` to run them, `qmlformat` and `qmllint` from Qt 6.12.0. No new vcpkg or Qt dependency, and nothing new at run time.

**Storage**: N/A. Four configuration files at the repository root (`.clang-format`, `.clang-tidy`, `.qmlformat.ini`, `.qmllint.ini`), `support/lint-requirements.txt`, and `.git-blame-ignore-revs`.

**Testing**: The existing Google Test suite via `just test` guards behaviour across the reformat and the lint fixes, run under the normal and the `C` locale. The checks themselves are validated with seeded violations in [quickstart.md](./quickstart.md). Headless runs of the application verify the QML changes.

**Target Platform**: The commands work on Linux, Windows and macOS. The automatic checks run on Linux x64.

**Project Type**: Desktop application (single CMake project, QML files at the repository root). This feature adds developer tooling to it.

**Performance Goals**: `just format` over the whole code base under 30 s; `just lint` under 5 minutes on a built tree (measured: clang-tidy 99 s with 8 jobs, qmllint 1.3 s); the automatic checks add no more than 5 minutes to a pull request (SC-002, SC-005).

**Constraints**: Formatting commits contain nothing but formatting (FR-006). String contents, including the `\uXXXX` icon escapes in QML, are untouched (FR-007; verified in research R3). Tool versions are fixed and a mismatch stops with a message (FR-021, FR-022). Local commands go through `just` in `build-cli/`.

**Scale/Scope**: 66 C++ files (14,887 lines) and 25 QML files (7,842 lines). About 2,000 lines change in the reformat; 159 C++ and 388 QML lint findings are resolved. New files: four configs, one CMake module, one requirements file, one blame-ignore file. Changed: `CMakeLists.txt`, `justfile`, `.github/workflows/build.yaml`, `README.md`, `CLAUDE.md`, the constitution, and most source files (layout), with code changes in 35 C++ and 20 QML files.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.* Checked against
constitution v1.3.0.

| Principle | Gate | Before research | After design |
|---|---|---|---|
| I. Simple, Focused Player | Features serve playing media. No new network use, accounts or clutter. | PASS. Development tooling; nothing changes for users. | PASS. No change to the shipped application's behaviour is intended; the lint fixes are covered by tests and headless runs. |
| II. Responsive UI Through Isolation | Slow work off the UI thread; failures surfaced. | PASS. Not affected. | PASS. Not affected. Qualifying QML names does not move work between threads. |
| III. Cross-Platform Parity | Same behaviour everywhere; platform code behind one interface; packaging keeps working. | PASS, with one point to resolve: can the commands and checks cover all three platforms? | PASS. The commands are CMake targets and run on all three. The automatic checks run on Linux x64. The two files that build on one platform only are formatted everywhere and exempt from clang-tidy, with the reason recorded (R9). The reformatted code still has to compile on every platform, which CI confirms. |
| IV. Pinned, Patchable Dependencies | Every dependency pinned; new ones justified. | PASS, with one point to resolve: where pinned lint tools come from. | PASS. Two development-only tools from PyPI, pinned to 22.1.8 in one file and run through `uvx`, which enforces the pin; the QML tools come from the pinned Qt (R1). Justification: nothing already in the project formats or analyses C++. `uv` is the one new tool a contributor installs; it is what makes the pin enforceable on all three platforms without a per-platform install step. They are not libraries and are not linked or shipped, so the vcpkg rule does not apply. |
| V. Tested Core Logic | Non-visual logic has tests; fixes get regression tests; UI has quickstart steps; CI green on all platforms. | PASS | PASS. No new application logic. Lint fixes are behaviour-preserving and run against the existing 210 tests on every platform. The checks are validated by seeded violations in the quickstart. |
| Technology Constraints | CMake, vcpkg, Ninja; `justfile` wraps the steps in `build-cli/`; quiet normal runs. | PASS | PASS. New `just` recipes follow the existing ones. |
| Development Workflow | Match the surrounding style; default to no comments; docs updated in the same change; issues and `<issue>-<slug>` branches. | PASS, with one point to resolve: suppression markers are comments. | PASS. The reformat keeps the style most of the code already uses. Suppression and opt-out markers with a reason are the one kind of comment this feature adds; the workflow rule is amended in the last step to say so and to point at the commands (R13). |

**Result**: Gates pass with no deviations. No unresolved clarifications remain.

## Project Structure

### Documentation (this feature)

```text
specs/003-code-lint-format/
├── plan.md              # This file
├── research.md          # Phase 0: decisions R1–R13, with measurements
├── data-model.md        # Phase 1: the configuration files and markers this feature introduces
├── quickstart.md        # Phase 1: validation, including the seeded violations for SC-003
├── contracts/
│   ├── commands.md      # just format, format-check and lint; the automatic checks
│   └── rules.md         # Style definitions, rule sets, suppressions, exemptions
├── checklists/
│   └── requirements.md
└── tasks.md             # Phase 2 output (/speckit-tasks; not created by /speckit-plan)
```

### Source Code (repository root)

```text
+ .clang-format                 # C++ style (R2)
+ .clang-tidy                   # C++ rule set, with reasons for excluded checks (R4)
+ .qmlformat.ini                # QML style (R3)
+ .qmllint.ini                  # QML rule set, warnings fatal (R5)
+ .git-blame-ignore-revs        # The two reformatting commits (R12)
+ support/lint.cmake            # format, format-check, lint-cpp, lint-qml and lint targets;
                                # file lists and the exemption list. Not part of the default build
+ support/lint-run.cmake        # Script the targets call: runs the tools through uvx at the pinned
                                # version, format check, suppression check, failure messages
+ support/lint-requirements.txt # clang-format==22.1.8, clang-tidy==22.1.8
~ CMakeLists.txt                # CMAKE_EXPORT_COMPILE_COMMANDS; include(support/lint.cmake)
~ justfile                      # format, format-check and lint recipes
~ .github/workflows/build.yaml  # setup-uv, "Check formatting" and "Lint" steps on the Linux x64 leg
~ .gitattributes                # LF line endings for C++ and QML sources
~ *.cpp, *.h, *.mm, tests/*.cpp # Reformat (all); lint fixes (35 files)
~ *.qml                         # Reformat (all); lint fixes (20 files)
~ AppConstants.qml              # Holds the icon font name, replacing a cross-file id
~ README.md, CLAUDE.md          # Commands, tool install, suppressions, rebasing across the reformat
~ .specify/memory/constitution.md  # Workflow rule amended (R13)
```

`+` new, `~` changed.

**Structure Decision**: Single project, flat layout, as before. The lint logic lives in one CMake
module under `support/`, next to the existing packaging module, and is reached only through
`just`. Configuration files sit at the repository root because that is where editors and the
tools themselves look for them.

## Delivery order

Each step is its own branch and pull request and leaves `main` building and passing. Issue #195
is the parent; sub-issues are created with the tasks. Branches are named `<issue-number>-<slug>`.

| Step | Content | Spec coverage | Source changes |
|---|---|---|---|
| 1. Tooling | The four config files, `support/lint.cmake`, the requirements file, the `just` recipes, compile-commands export, contributor docs. The commands exist and report the known violations; nothing is enforced. | FR-001 to FR-004, FR-010 to FR-012, FR-017, FR-021 to FR-023 | None |
| 2. Reformat (Story 1) | `just format` as two commits (C++, QML) and `.git-blame-ignore-revs`. | FR-005 to FR-009 | Layout only, about 2,000 lines |
| 3. C++ lint (Story 2) | The 159 findings: automatic fixes first, one commit per check, then the manual ones. | FR-013 to FR-016 (C++) | 35 files |
| 4. QML lint (Story 2) | The 388 findings, one component at a time, each verified with a headless run. | FR-013, FR-014 (QML) | 20 files |
| 5. Enforcement (Story 3) | `setup-uv`, "Check formatting" and "Lint" steps in CI; the seeded-violation run; the CI timing check against SC-005. | FR-019, FR-020, SC-003, SC-005 | None |
| 6. Wrap-up | Constitution amendment, final documentation, the full quickstart. | FR-024, SC-001 to SC-007 | None |

Steps 3 and 4 do not depend on each other and can be done in either order. Step 2 should land
when no other branch is open, because it touches most files. Until step 5, each step ends by
running `just format-check` and the lint recipes it has made clean, so nothing slips back before
the checks are automatic.

## Risks

| Risk | Mitigation |
|---|---|
| The reformat or a lint fix changes behaviour. | Formatting commits are made by the tool alone. The 210 tests run before and after each step, under both locales, on every platform in CI. QML fixes are verified per component with headless runs. |
| Qualifying QML names breaks a binding that relied on scope lookup. | Step 4 goes one component at a time; `qmllint --fix` is applied only where it is certain, and the three cross-file ids are replaced deliberately (R5). |
| clang-tidy takes longer on the CI runner than estimated. | The 3 to 4 minute figure is an estimate (R8). If the first run exceeds the 5-minute budget, the options are more parallel jobs or caching the per-file results. |
| Open branches conflict with the reformat. | Land step 2 with no other branch open; the docs describe rebasing across it (R12). |
| Between the tooling and reformat pull requests the configs are at the repository root but the code does not match them yet, so an editor with format-on-save would reformat whole files in an unrelated change. | Merge the two back to back. |
| A contributor's tool version differs. | The normal path runs the pinned version through `uvx`, so it cannot differ. With an overridden binary the targets stop with a message naming both versions (R11). |

## Complexity Tracking

No violations.
