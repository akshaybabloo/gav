---

description: "Task list for Code Linting and Formatting"
---

# Tasks: Code Linting and Formatting

**Input**: Design documents from `specs/003-code-lint-format/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md)

**Tests**: No new unit tests. This feature adds no application logic; the existing suite
(`just test`, also under `LC_ALL=C LANG=C`) is the guard that the reformat and the lint fixes
change no behaviour, and the checks themselves are validated with the seeded violations in
[quickstart.md](./quickstart.md#seeded-violations-sc-003).

**Parent issue**: #195. Each phase after the first names the step of
[plan.md → Delivery order](./plan.md#delivery-order) it delivers; sub-issues are created under
#195 before a step starts, and its branch is named `<issue-number>-<slug>`.

**Organization**: Tasks are grouped by user story. Phases 1 and 2 are the tooling step, with no
source changes. Each phase after that is one pull request, except Story 2, which is two (C++ and
QML).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on unfinished tasks)
- **[Story]**: The user story the task belongs to (US1–US3)

## Path Conventions

GAV keeps every source file at the repository root. QML files belong to the `gavqml` module, and
C++ units are registered in `qt_add_qml_module(appgav … SOURCES …)` in `CMakeLists.txt`. Tests
live in `tests/` and are compiled into the `gav_tests` target. Build, test, format and lint only
through `just`, which uses `build-cli/`. Python-distributed tools are run with `uv` / `uvx`.
Icons in QML stay as `"\uXXXX"` escapes.

---

## Phase 1: Setup (Shared Infrastructure)

**Step**: 1, Tooling (with Phase 2) · **Issue**: #195 · **Branch**: `195-code-lint-format`

**Purpose**: The pins and the build wiring every later phase needs

- [X] T001 [P] Create `support/lint-requirements.txt` with exactly two lines, `clang-format==22.1.8` and `clang-tidy==22.1.8` ([data-model.md → Tool pin](./data-model.md#tool-pin): "one line per tool, `name==version`").
- [X] T002 In `CMakeLists.txt`: set `CMAKE_EXPORT_COMPILE_COMMANDS` to `ON` next to `CMAKE_CXX_STANDARD`; move the `QML_FILES` list of `qt_add_qml_module(appgav …)` into a variable `GAV_QML_FILES` and pass that variable; add `include(support/lint.cmake)` after both `appgav` and, when `BUILD_TESTS` is on, `gav_tests` are defined.
- [X] T003 Create `support/lint.cmake` with the file lists only, per [research R6](./research.md):
  - `GAV_LINT_CPP_SOURCES`: the `SOURCES` of `appgav` and of `gav_tests` (when it exists) that end in `.cpp`, `.h` or `.mm`, lie under `CMAKE_SOURCE_DIR` and not under `CMAKE_BINARY_DIR` or `vcpkg-ports/`, without duplicates
  - `GAV_PLATFORM_ONLY_SOURCES`: `mediasession_windows.cpp` and `mediasession_macos.mm`, each with its reason in a comment ("builds on Windows only; lint runs on Linux", "builds on macOS only; lint runs on Linux"), as in [contracts/rules.md](./contracts/rules.md#files-exempt-from-the-c-linter)
  - `GAV_FORMAT_CPP_FILES`: the two lists together, so platform-only files are formatted on every platform
  - `GAV_LINT_QML_FILES`: `GAV_QML_FILES` as absolute paths

---

## Phase 2: Foundational (Blocking Prerequisites)

**Step**: 1, Tooling (continued)

**Purpose**: How the tools are run, shared by every command

**⚠️ CRITICAL**: No story can start until the tools can be invoked at their pinned versions

- [X] T004 In `support/lint.cmake`, resolve the C++ tools per [research R11](./research.md):
  - read the two versions from `support/lint-requirements.txt`
  - default: `find_program(uvx)`; the tool commands are `uvx --from clang-format==<version> clang-format` and `uvx --from clang-tidy==<version> clang-tidy`
  - if `uvx` is not found and no override is set, the targets (not the configure step) fail with one message that says uv is needed and gives `https://docs.astral.sh/uv/`
  - cache variables `GAV_CLANG_FORMAT` and `GAV_CLANG_TIDY` override the command with a given binary; then the target first compares `<binary> --version` with the pinned version and fails with "found X, need Y" on a mismatch
  - find `qmlformat` and `qmllint` next to the Qt in use (`QT6_INSTALL_PREFIX`/`bin`)
- [X] T005 [P] Create `support/lint-run.cmake`, the script every target calls with `cmake -P`, starting with its `markers` action: over a file list passed in, it prints `file:line: message` and fails when a first-party file has: `NOLINT` or `NOLINTNEXTLINE` without `(check-name)` or without `: reason` after it; `clang-format off` without `: reason`; `qmllint disable` without a category, or without a comment line directly above it; `qmlformat off` without a comment line directly above it ([research R10](./research.md), [contracts/rules.md](./contracts/rules.md#suppressing-one-finding)).
- [X] T006 In `support/lint.cmake` and `support/lint-run.cmake`, add a `lint-versions` target (the script's `versions` action) that the lint targets depend on and that prints the versions of clang-format, clang-tidy, qmlformat and qmllint in use; the format actions print the versions of the two formatters themselves ([contracts/commands.md](./contracts/commands.md#local-commands): "Each command starts by printing the versions of the tools it uses").
- [X] T007 Add recipes to `justfile`, each starting with `@{{ configure_if_needed }}` and then building one target in `{{ build_dir }}`, with a one-line description comment like the existing recipes: `format`, `format-cpp`, `format-qml`, `format-check`, `lint`, `lint-cpp`, `lint-qml`. They must work with the PowerShell setting at the top of the file.

**Checkpoint**: The tools can be run at their pinned versions and the recipes exist. Their targets are added by the first tasks of Phases 3 and 4.

---

## Phase 3: User Story 1 - Format a change with one command (Priority: P1) 🎯 MVP

**Goal**: One style per language, one command to apply it, and the existing code in that style.

**Independent Test**: `just format-check` reports nothing on a clean checkout; a hand-misformatted
C++ file and QML file are restored exactly by `just format` (quickstart Q1.1–Q1.9).

### Style definitions and commands (Step 1, Tooling; same branch as Phases 1–2)

- [X] T008 [P] [US1] Create `.clang-format` with exactly the settings in [research R2](./research.md): `BasedOnStyle: LLVM`, `IndentWidth: 4`, `ColumnLimit: 160`, `AccessModifierOffset: -4`, `PointerAlignment: Right`, `SortIncludes: Never`, `LineEnding: LF`.
- [X] T009 [P] [US1] Create `.qmlformat.ini` from `qmlformat --write-defaults` and set, per [research R3](./research.md): indent width 4, spaces not tabs, no maximum column width, normalisation off, group-attributes-together off, objects-spacing off, functions-spacing off, newline type `unix`.
- [X] T010 [US1] In `support/lint.cmake`, add the targets `format-cpp` (`clang-format -i` over `GAV_FORMAT_CPP_FILES`), `format-qml` (`qmlformat -i` over `GAV_LINT_QML_FILES`) and `format` (both). None of the targets this feature adds is part of the default build (`ALL`), the install step or the packages (FR-023).
- [X] T011 [US1] In `support/lint.cmake`, add `format-check`: `clang-format --dry-run --Werror` over the C++ list; for QML, the `format-check` action of `support/lint-run.cmake` formats each file to memory, compares it with the file, and prints each file that differs. On failure the last line is "Run `just format` to fix." Nothing is modified ([contracts/commands.md](./contracts/commands.md#local-commands)).
- [X] T012 [US1] Check the tooling step on the unformatted tree: `just format-check` exits non-zero and lists files; the C++ files it lists include `custommediaplayer.cpp` and `custommediaplayer.h`; no file under `build-cli/` or `vcpkg-ports/` is listed; `git status` shows no source file modified; `just build` and `just test` run no formatter or linter.

### Reformat the existing code (Step 2; own issue and branch, opened when no other branch is open)

- [ ] T013 [US1] Record the baseline before touching anything: `just test` and `LC_ALL=C LANG=C just test` pass; note the count of `\uXXXX` escapes per QML file (`grep -c 'u[0-9a-f]\{4\}' *.qml`). Then, in its own commit, add `*.cpp`, `*.h`, `*.mm` and `*.qml` to `.gitattributes` as `text eol=lf`, so a Windows checkout has the line endings both styles write ([research R12](./research.md)).
- [ ] T014 [US1] Run `just format-cpp` and commit the result alone as "Reformat C++ sources with clang-format 22.1.8". The commit must contain nothing but what the tool produced (FR-006). Then `just build`, `just test` and `LC_ALL=C LANG=C just test` must pass.
- [ ] T015 [US1] Run `just format-qml` and commit the result alone as "Reformat QML sources with qmlformat (Qt 6.12.0)". Confirm the escape counts from T013 are unchanged (FR-007), `just build` succeeds, and a headless, silent run of the application starts with no QML warnings in its log.
- [ ] T016 [US1] Create `.git-blame-ignore-revs` with the full ids of the two commits from T014 and T015, each under a comment line saying what it reformatted ([data-model.md → Reformatting change](./data-model.md#reformatting-change)). The pull request must be merged with a merge commit so the ids stay valid ([research R12](./research.md)).
- [ ] T017 [US1] Validate quickstart [Q1.1–Q1.9](./quickstart.md#story-1-format-a-change-with-one-command). Fix what fails. `just format-check` must exit 0.

**Checkpoint**: Story 1 works on its own. Every first-party file matches its style.

---

## Phase 4: User Story 2 - Catch likely defects before review (Priority: P2)

**Goal**: One rule set per language, one command to run them, and no findings in the existing code.

**Independent Test**: `just lint` reports nothing on a clean, built checkout; a seeded problem in a
C++ file and one in a QML file are each reported with file, line and rule (quickstart Q2.1–Q2.9).

### Rule sets and commands (Step 1, Tooling; same branch as Phases 1–2)

- [X] T018 [P] [US2] Create `.clang-tidy` per [research R4](./research.md) and [contracts/rules.md](./contracts/rules.md#c-rule-set-clang-tidy): the `Checks` list exactly as given there (the four full groups, the three exclusions, `cert-err33-c`, and the named `modernize-`, `readability-`, `misc-` and `cppcoreguidelines-` checks); `WarningsAsErrors: '*'`; `HeaderFilterRegex` and `ExcludeHeaderFilterRegex` that leave out build directories and vcpkg trees (the `lint-cpp` target passes exact filters built from the real source and build directories); `FormatStyle: file`; a comment above each of the three excluded checks giving its reason from R4.
- [X] T019 [P] [US2] Create `.qmllint.ini` from `qmllint --write-defaults`, keeping every category at its default severity except `UnusedImports=warning` (FR-011; at the default, info, the warning limit does not count it) and setting `MaxWarnings=0` ([research R5](./research.md)).
- [X] T020 [US2] In `support/lint.cmake`, add `lint-cpp`: one custom command per `.cpp` in `GAV_LINT_CPP_SOURCES` that runs clang-tidy with `--quiet` against that file's own entry of the compile commands (so a source shared by `appgav` and `gav_tests` is analysed once) and writes its findings to a result file; each depends on its source, on `.clang-tidy` and on every first-party header; the target then prints the findings of all result files, without duplicates, and fails if there are any; the target depends on `appgav` and `gav_tests` being built ([research R7](./research.md)). Files in `GAV_PLATFORM_ONLY_SOURCES` get no command.
- [X] T021 [US2] In `support/lint.cmake`, add `lint-qml` (qmllint with the same import paths and resource files Qt's generated `appgav_qmllint` target uses, reading `.qmllint.ini`) and `lint` (`lint-cpp`, `lint-qml` and the marker check from T005 over all first-party files). On failure the last line is "Run `just lint` to reproduce."
- [X] T022 [US2] Check the tooling step on the unfixed tree: `just lint-cpp` reports findings as `file:line: message [check]` and exits non-zero, with a count close to the 159 in research R4; `just lint-qml` reports close to 388; neither modifies a file; `mediasession_windows.cpp` and `mediasession_macos.mm` are not analysed.
- [X] T023 [US2] Add a "Formatting and linting" part to the development section of `README.md` and to `CLAUDE.md`: uv as the one thing to install, the five recipes, and that `just lint` builds first ([contracts/commands.md](./contracts/commands.md)).

**Checkpoint**: The tooling step is complete: all commands exist, no source file has changed. Ships as its own PR.

### C++ findings (Step 3; own issue and branch; after Step 2)

- [ ] T024 [US2] Apply the automatic fixes, one check per commit, with `uvx --from clang-tidy==22.1.8 run-clang-tidy.py -p build-cli -fix -checks='-*,<check>'` restricted to first-party files, in this order: `readability-braces-around-statements` (45), `performance-avoid-endl` (13), `modernize-use-using` (11), `readability-inconsistent-declaration-parameter-name` (9), `cppcoreguidelines-prefer-member-initializer` (9). After each: read the diff, run `just format`, `just build` and `just test`, then commit. Formatting the lines a fix touched belongs in that fix's commit; what FR-015 rules out is mixing lint fixes into the bulk reformat.
- [ ] T025 [US2] Fix by hand, in their own commit, the integer-conversion findings: `bugprone-narrowing-conversions` (17), `bugprone-implicit-widening-of-multiplication-result` (16) and `bugprone-misplaced-widening-cast` (3). Prefer a wider type or an explicit conversion at the point of use; do not change a function's behaviour for values it already handles.
- [ ] T026 [US2] Fix by hand the remaining 36 findings across 17 checks (list them with `just lint-cpp`), starting with `custommediaplayer.cpp`. Where a finding is a false positive or intentional, suppress it at that line with `// NOLINT(check-name): reason` ([contracts/rules.md](./contracts/rules.md#suppressing-one-finding)); do not disable a check project-wide without adding its reason to `.clang-tidy` (FR-014).
- [ ] T027 [US2] Verify the C++ step: `just lint-cpp` exits 0; `just format-check` exits 0; `just test` and `LC_ALL=C LANG=C just test` pass; a headless, silent run plays a local file and a playlist without new warnings.

**Checkpoint**: C++ is lint-clean. Ships as its own PR.

### QML findings (Step 4; own issue and branch; after Step 2, independent of Step 3)

- [ ] T028 [US2] Replace the cross-file icon font id: add the font loader and a `readonly property string iconFont` to the `AppConstants.qml` singleton; replace every `materialSymbolsOutlined.name` in the 14 QML files that use it with `AppConstants.iconFont`; remove the three `FontLoader { id: materialSymbolsOutlined }` objects from `Main.qml`, `TitleBar.qml` and `MiniPlayerWindow.qml`. Verify headless that icons still render in the main window, the title bar and the mini player.
- [ ] T029 [US2] Qualify names that belong to a component's own root object, one file per commit in descending order of findings (`MediaControlsComponent.qml` 132, `SeekBarComponent.qml` 52, `Main.qml` 50, `MediaComponent.qml` 34, `TrackMenu.qml` 24, then the rest): run `qmllint --fix` on the file, read the diff, finish by hand, run `just format-qml`. After each file, a headless, silent run exercises that component and its log has no new QML warnings.
- [ ] T030 [US2] Replace the two remaining cross-file ids, `mainWindow` (21 uses) and `miniPlayerWindow` (9), with properties or signals on the components that use them, set from `Main.qml`. Behaviour to re-check headless: control auto-hide with the playlist panel open, full screen, the mini player toggle.
- [ ] T031 [US2] Resolve the other categories: `modelData` (7) becomes a `required property` in its delegate; `missing-property` (10); `id-shadows-member` (8, rename the id or the property, for example the `playList` id against the `playList` property); block-scoped `var` (6) becomes `let`; unused imports (2) are removed. Suppress only with a reason line above `// qmllint disable category`.
- [ ] T032 [US2] Verify the QML step: `just lint-qml` and `just lint` exit 0; `just format-check` exits 0; the escape counts from T013 are unchanged; `just test` passes; headless, silent runs of the Story 1 to Story 4 checks of `specs/002-playlist-redesign/quickstart.md` that can be simulated still pass (panel, rows, search and grouping, selection and keyboard).
- [ ] T033 [US2] Validate quickstart [Q2.1–Q2.9](./quickstart.md#story-2-catch-likely-defects-before-review). Fix what fails.

**Checkpoint**: Stories 1 and 2 work. `just format-check` and `just lint` both report nothing.

---

## Phase 5: User Story 3 - Every change is checked automatically (Priority: P3)

**Step**: 5, Enforcement · **Issue**: to be created under #195

**Goal**: Both checks run on every pull request and on `main`, and a failure blocks the merge.

**Independent Test**: A pull request with one formatting violation and one lint finding fails both
checks with a message that says what and where; fixing them makes it pass (quickstart Q3.1–Q3.5).

- [ ] T034 [US3] In `.github/workflows/build.yaml`, on the Linux x64 leg only (`matrix.os == 'ubuntu-24.04'`): add the `astral-sh/setup-uv` action before the configure step; add a "Check formatting" step right after "Configure CMake (Linux/macOS)" that builds the `format-check` target; add a "Lint" step right after "Build" that builds the `lint` target ([research R8](./research.md)). Neither step applies fixes or pushes anything.
- [ ] T035 [US3] Confirm from the first run of the workflow that each step prints the tool versions and that they match `support/lint-requirements.txt` and Qt 6.12.0 (FR-021), and compare the Linux x64 leg's duration with a run from before this phase: the two steps together must add no more than 5 minutes (SC-005). If "Lint" takes longer, raise its parallel jobs or cache the per-file stamps, and note the measured time in [research R8](./research.md), replacing the estimate.
- [ ] T036 [US3] Run the seeded violations of [quickstart.md](./quickstart.md#seeded-violations-sc-003) on a throwaway branch and pull request: all ten must be reported, each with its file and, for lint, its line and rule; each failing step's log must end with the local command (FR-020). Close the pull request and delete the branch afterwards.
- [ ] T037 [US3] Validate quickstart [Q3.1–Q3.5](./quickstart.md#story-3-every-change-is-checked-automatically). Fix what fails.

**Checkpoint**: All three stories work. Ships as its own PR.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Step**: 6, Wrap-up · **Issue**: to be created under #195

**Purpose**: Governance, documentation and full validation

- [ ] T038 Amend `.specify/memory/constitution.md` with `/speckit-constitution` per [research R13](./research.md): the Development Workflow style rule says that layout is what `just format` produces, that `just lint` must pass, and that suppression and opt-out markers with a reason are allowed comments. Then update the Constitution Check in `specs/003-code-lint-format/plan.md` to cite the new version.
- [ ] T039 [P] Finish the contributor documentation in `README.md` and `CLAUDE.md` (FR-024): how to suppress a finding and keep a hand-made layout, the files exempt from the C++ linter and why, `git config blame.ignoreRevsFile .git-blame-ignore-revs`, and how to rebase a branch across the reformat ([research R12](./research.md)).
- [ ] T040 Run the whole of [quickstart.md](./quickstart.md), including the timings for SC-002, and record the results in the pull request description. The byte-identical formatting check on Windows or macOS (SC-006) needs a checkout on one of those platforms; say so if it was not run.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: no dependencies
- **Foundational (Phase 2)**: needs Setup. Blocks every story
- **US1 (Phase 3)**: its style and command tasks (T008–T012) need Foundational and belong to the tooling PR. Its reformat tasks (T013–T017) need the tooling PR merged
- **US2 (Phase 4)**: its rule-set and command tasks (T018–T023) need Foundational and belong to the tooling PR. Its fix tasks need the reformat merged, so that fixes are not mixed with layout changes. The C++ block (T024–T027) and the QML block (T028–T033) are independent of each other
- **US3 (Phase 5)**: needs US1 and both blocks of US2, because the checks must pass before they gate merges
- **Polish (Phase 6)**: after US3

### User Story Dependencies

```text
Setup → Foundational → tooling PR (T008–T012, T018–T023)
                         └→ US1 reformat → US2 C++ fixes ─┐
                                         └→ US2 QML fixes ─┴→ US3 → Polish
```

### Within Each User Story

- Configuration before the target that reads it
- A formatting commit contains nothing but formatting; a lint fix is never in the same commit
- After every fix commit: `just format`, `just build`, `just test`
- `support/lint.cmake` is edited by one task at a time

### Parallel Opportunities

- Setup: T001 alongside T002
- Foundational: T005 alongside T004
- Tooling PR: T008, T009, T018 and T019 together (four different config files)
- After the reformat: the C++ block and the QML block can proceed on separate branches
- Polish: T039 alongside T038

---

## Parallel Example: Tooling PR

```bash
# The four configuration files, together:
Task: ".clang-format per research R2"                      # T008
Task: ".qmlformat.ini per research R3"                     # T009
Task: ".clang-tidy per research R4"                        # T018
Task: ".qmllint.ini per research R5"                       # T019

# Then, one at a time, in support/lint.cmake:
Task: "format-cpp, format-qml and format targets"          # T010
Task: "format-check target"                                # T011
Task: "lint-cpp target"                                    # T020
Task: "lint-qml and lint targets"                          # T021
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Phases 1 and 2 plus the tooling tasks of Phases 3 and 4: the commands exist, nothing changes
2. The reformat (T013–T017)
3. Stop and validate Q1.1–Q1.9. From here every file has one layout and `just format` keeps it

### Incremental Delivery

1. Tooling PR: configs and commands, no source changes
2. Reformat PR: two formatting-only commits and the blame-ignore file
3. C++ lint PR: 159 findings
4. QML lint PR: 388 findings
5. Enforcement PR: the two CI steps and the seeded-violation run
6. Wrap-up PR: constitution amendment and documentation

Each PR passes `just test` and CI on all platforms before merge. Until the enforcement PR, each
one ends with `just format-check` and the lint recipes already made clean.

---

## Notes

- [P] tasks touch different files and do not wait on unfinished tasks
- Finding counts are from research on `main` at `4f8e086`; they will drift a little as the code changes
- Lint fixes must not change behaviour; when one would, suppress with a reason and open an issue instead
- Windows and macOS are first compiled by CI on the PR
- Commit after each task or logical group, on a branch, never on `main`
