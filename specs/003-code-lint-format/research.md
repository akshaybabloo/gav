# Research: Code Linting and Formatting

Decisions behind [plan.md](./plan.md). Measurements were taken on 2026-10-07 on the `main` branch
at `4f8e086` (66 first-party C++ files, 14,887 lines; 25 QML files, 7,842 lines), on a Linux x64
development machine with 8 parallel jobs. Tools were installed into a scratch environment; nothing
in the repository was changed to take them.

## R1. Which tools, and which versions (FR-021, FR-023)

**Decision**

| Purpose | Tool | Version | Where it comes from |
|---|---|---|---|
| C++ formatting | `clang-format` | 23.1.0 | The machine: `clang-format` on `PATH`, or the binary named by `GAV_CLANG_FORMAT` |
| C++ linting | `clang-tidy` | 23.1.0 | The machine: `clang-tidy` on `PATH`, or the binary named by `GAV_CLANG_TIDY` |
| QML formatting | `qmlformat` | Qt 6.12.0 | The Qt installation the project already pins |
| QML linting | `qmllint` | Qt 6.12.0 | The same |

The version of the two C++ tools is fixed in one place, `GAV_LLVM_VERSION` in
`support/lint.cmake`. The commands do not fetch anything: they use the binaries that are
installed, check that each reports exactly that version, and stop with the version found and the
version needed if it does not (R11).

The maintainer chose this: 23.1.0 is the version they already use, and they plan to publish
builds of the two tools for contributors and CI later. Until those exist, the binaries of the
LLVM 23.1.0 release are the ones to install.

**Rationale**

- The issue asks for clang-tidy. `clang-format` is its companion and shares its configuration
  conventions, and `qmlformat` and `qmllint` are Qt's own tools, already present in every
  contributor's and CI's Qt 6.12.0.
- One version for both C++ tools, checked before every run, keeps the pin enforced without a
  package manager in between: a different clang-format version would format the same file
  differently, which FR-022 and SC-006 rule out.
- Measured on `main` at `4f8e086`: clang-format 23.1.0 and 22.1.8 give byte-identical output on
  all 66 C++ files. clang-tidy 23.1.0 reports every finding 22.1.8 does, plus 13 from two checks
  that are new in the enabled groups (R4).
- Distribution packages are not pinned: Ubuntu 24.04 offers 18, Homebrew and Chocolatey offer
  whatever is current. The version check turns those into a clear message.

**Alternatives considered**

- PyPI wheels run through `uvx`, with the versions in a requirements file: the first design. It
  needs no install step, but PyPI has no clang-tidy newer than 22.1.8, so it cannot provide the
  chosen version. Removed at the maintainer's request.
- System packages (`apt`, `brew`, `choco`): unpinned, different per platform.
- LLVM's own apt repository: Linux only, and it carries the latest point release of a branch
  rather than a chosen one, so it cannot provide exactly 23.1.0 once 23.1.1 is out.
- vcpkg's `llvm` port with the `clang` and `clang-tools-extra` features: it builds both tools,
  but from source, which is LLVM and clang for every contributor and CI leg unless it is made an
  opt-in manifest feature, and again on every binary-cache miss. The pinned `VCPKG_COMMIT` has
  18.1.6 and current vcpkg has 23.1.2, so it would also move the pin away from 23.1.0. Not
  measured. The maintainer chose published binaries instead.
- clazy (Qt-aware C++ checks): would need its own build. Left for later.

## R2. C++ style (FR-001, FR-002, FR-005)

**Decision**: `.clang-format` at the repository root:

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 160
AccessModifierOffset: -4
PointerAlignment: Right
SortIncludes: Never
LineEnding: LF
```

**Measured**: lines the formatter would change, per candidate.

| Candidate | Lines changed | Share |
|---|---|---|
| LLVM, indent 4, limit 160 | 1,712 | 11.5 % |
| LLVM, indent 4, limit 140 | 1,735 | 11.7 % |
| LLVM, indent 4, limit 120 | 1,910 | 12.8 % |
| Google, indent 4, limit 160 | 2,014 | 13.5 % |
| WebKit, indent 4, limit 160 | 2,904 | 19.5 % |
| LLVM, indent 2, limit 160 | 10,743 | 72.2 % |

**Rationale**

- Of the 1,712 lines, 1,281 are in `custommediaplayer.cpp` and `custommediaplayer.h`, the only two
  files that use two-space indentation. Every other file already uses four. Outside those two
  files the chosen style changes about 430 lines in 64 files (3 %), mostly long calls that were
  wrapped by hand and now fit on one line.
- `SortIncludes: Never` keeps include order as written. Reordering includes is a code change, not
  a layout change (FR-006), and some files rely on their order.
- `LineEnding: LF`, and the same for QML, so a Windows checkout formats to the same bytes (SC-006).
- `InsertBraces` is left off. Adding braces changes tokens, so it is done as a reviewed lint fix
  (R4), not by the formatter.

## R3. QML style (FR-001, FR-002, FR-007, FR-008)

**Decision**: `.qmlformat.ini` at the repository root with qmlformat's defaults made explicit:
indent width 4, spaces, no column limit, no attribute normalisation.

**Measured**: lines `qmlformat` would change, per option set.

| Options | Lines changed | Share |
|---|---|---|
| Defaults (indent 4) | 283 | 3.6 % |
| `--group-attributes-together` | 521 | 6.6 % |
| `--normalize` | 1,064 | 13.6 % |
| `--normalize --objects-spacing --functions-spacing` | 1,159 | 14.8 % |

**Rationale**

- The QML files are already close to qmlformat's default output. Their attributes are in
  alphabetical order, which is not the order `--normalize` produces, so normalising would rewrite
  the most lines for no gain.
- Of the 283 lines, 178 are in `PlaylistPanel.qml`, where a nested layout was added without
  re-indenting its children.
- **Icon escapes are safe**: across all 25 files the number of `\uXXXX` sequences is identical
  before and after formatting (FR-007).
- **Opt-out exists**: text between `// qmlformat off` and `// qmlformat on` is left as written
  (checked with a hand-aligned array). `clang-format` has the same with `// clang-format off` /
  `on` (FR-008). R10 says where the reason goes.
- `qmlformat` has no check mode. The format check formats each file to memory and compares it with
  the file (R6).

## R4. C++ rule set (FR-010, FR-011, FR-013)

**Decision**: `.clang-tidy` lists its checks explicitly, with `WarningsAsErrors: '*'` and a header
filter that matches only first-party headers.

- All of `clang-analyzer-*`, `bugprone-*`, `performance-*` and `portability-*`, minus
  `bugprone-easily-swappable-parameters`, `bugprone-throwing-static-initialization` and
  `performance-enum-size`.
- `cert-err33-c`.
- Selected `modernize-` checks: `use-override`, `use-nullptr`, `use-using`, `use-emplace`,
  `redundant-void-arg`, `deprecated-headers`, `loop-convert`, `make-unique`, `make-shared`,
  `use-bool-literals`, `use-equals-default`, `use-equals-delete`, `pass-by-value`,
  `raw-string-literal`.
- Selected `readability-` checks: `braces-around-statements`,
  `inconsistent-declaration-parameter-name`, `else-after-return`, `qualified-auto`,
  `static-accessed-through-instance`, `container-size-empty`, `simplify-boolean-expr`,
  `misleading-indentation`, `redundant-control-flow`, `redundant-declaration`,
  `redundant-member-init`, `redundant-smartptr-get`, `redundant-string-cstr`,
  `redundant-string-init`, `delete-null-pointer`, `duplicate-include`,
  `misplaced-array-index`, `non-const-parameter`, `string-compare`, `uniqueptr-delete-release`.
- Selected `misc-` checks: `unused-parameters`, `unused-using-decls`, `definitions-in-headers`,
  `redundant-expression`, `misplaced-const`, `new-delete-overloads`, `non-copyable-objects`,
  `throw-by-value-catch-by-reference`, `unconventional-assign-operator`,
  `uniqueptr-reset-release`, `static-assert`, `override-with-different-visibility`.
- Selected `cppcoreguidelines-` checks: `init-variables`, `pro-type-member-init`, `slicing`,
  `virtual-class-destructor`, `prefer-member-initializer`.

**Measured**

| Rule set | Unique findings | Files | Time |
|---|---|---|---|
| Broad (every check in bugprone, performance, modernize, readability, misc, clang-analyzer, cppcoreguidelines, portability, cert) | 3,090 | 40 | 103 s |
| Curated (above) | 161 | 36 | 99 s |

The broad run was made with clang-tidy 22.1.8; the curated figures are from 23.1.0. The 161
findings under the curated set:

| Findings | Check | How they get resolved |
|---|---|---|
| 45 | `readability-braces-around-statements` | Automatic fix |
| 17 | `bugprone-narrowing-conversions` | By hand: explicit conversions, or a wider type |
| 16 | `bugprone-implicit-widening-of-multiplication-result` | By hand |
| 13 | `performance-avoid-endl` | Automatic fix |
| 11 | `modernize-use-using` | Automatic fix |
| 9 | `readability-inconsistent-declaration-parameter-name` | Automatic fix |
| 9 | `cppcoreguidelines-prefer-member-initializer` | Automatic fix, reviewed |
| 41 | 19 other checks, 1 to 5 each | By hand |

`custommediaplayer.cpp` has 48 of them; no other file has more than 15.

**Rationale**

- The curated set keeps the checks that find defects and drops the ones that are opinions about
  style. The four largest sources of noise in the broad run are `modernize-use-trailing-return-type`
  (727), `misc-include-cleaner` (494, which does not understand Qt's umbrella headers),
  `cppcoreguidelines-pro-bounds-avoid-unchecked-container-access` (354) and the magic-number checks
  (316 each).
- `clang-analyzer-*` reports nothing today, so turning it on costs no clean-up and guards against
  regressions.
- The four excluded checks from otherwise-enabled groups, with the reasons that go into
  `.clang-tidy` (FR-014): swappable parameters fires on any two arguments of the same type;
  throwing static initialisation fires on every file-scope `QString` or `QRegularExpression`,
  which is how Qt code is written; enum size is a micro-optimisation; signed bitwise, new in the
  `bugprone` group in 23, fires on every test of a flag that FFmpeg or Qt declares as a signed
  integer (11 findings, none of them a defect).
- The other check that 23 adds findings for, `performance-use-std-move` (2), stays on.
- The compile commands produced by GCC are accepted by clang-tidy as they are: the broad run had
  no compile errors.
- Four groups are enabled by wildcard. That is still a fixed list, because the tool version is
  pinned (R1); raising the pin can add checks to those groups and is therefore a change to the
  rule set, made with whatever fixes it needs.

**Alternatives considered**

- Turning everything on and suppressing: 3,090 suppressions would bury the real findings.
- A minimal set (`clang-analyzer-*` and `bugprone-*` only): leaves out cheap, mechanical wins such
  as `performance-*` and missing braces.

## R5. QML rule set (FR-010, FR-011, FR-013)

**Decision**: `.qmllint.ini` at the repository root with every category at its default severity,
except `UnusedImports`, which is raised from info to warning, and `MaxWarnings=0`, so that any
warning fails the check. qmllint reports unused imports as information by default, which the
warning limit does not count; FR-011 asks for them to be covered.

**Measured**: `qmllint` over the module as the build runs it takes 1.3 s and reports 388 findings
in 20 files.

| Findings | Category | Meaning |
|---|---|---|
| 362 | `unqualified` | A name is used without saying which object it belongs to |
| 10 | `missing-property` | A member is used on a type that does not declare it |
| 8 | `id-shadows-member` | An `id` has the same name as a property |
| 6 | (no category) | `var` declared in a block but visible in the whole function |
| 2 | `unused-imports` | |

The unqualified names: `player` (85), `materialSymbolsOutlined` (33), `root` (25), `repeatMode`
(22), `mainWindow` (21), `subtitles` (16), and about 30 others. `MediaControlsComponent.qml` has
132 of the 388.

**How they get resolved**

- Names that belong to the component's own root object get the `root.` prefix. `qmllint --fix`
  applies the suggestions it is sure of; the rest are done by hand.
- Three names are ids from another file, reached through the object tree: the icon font
  (`materialSymbolsOutlined`, 33 uses), `mainWindow` (21) and `miniPlayerWindow` (9). The icon
  font's name moves into the existing `AppConstants` singleton. The other two become properties or
  signals of the components that use them.
- `modelData` (7) becomes a `required property` in its delegate.

**Rationale**

- `unqualified` is the category that finds real mistakes (a typo resolves to nothing at run time
  and fails silently), and FR-011 names unresolved names explicitly.
- It is also the largest piece of work in this feature and the one that can change behaviour, so
  it is its own delivery step (see the plan), verified component by component.

**Alternative considered**: leaving `unqualified` off for now. That would make the QML check
nearly empty (26 findings) and leave out the category the requirement is about.

## R6. How the commands are built (FR-003, FR-004, FR-012, FR-017)

**Decision**: The work is done by CMake targets defined in a new `support/lint.cmake`, and the
`justfile` gets thin recipes that build those targets. The targets call one script,
`support/lint-run.cmake`, which knows how to run each tool at its pinned version, so that every
command reports failures the same way. None of the targets is part of the default build, the
install step or the packages.

| Recipe | Target | What it runs |
|---|---|---|
| `just format` | `format` | `clang-format -i` and `qmlformat -i` over the first-party file lists (`just format-cpp` and `just format-qml` do one language) |
| `just format-check` | `format-check` | `clang-format --dry-run --Werror`; for QML, format to memory and compare |
| `just lint` | `lint` | `lint-cpp` (clang-tidy, one command per source file) and `lint-qml` (qmllint), plus the suppression check (R10) |

**Rationale**

- CMake already knows the file lists: the sources of `appgav` and `gav_tests` and the module's
  `QML_FILES`. Taking them from there means a new file is covered without touching a second list,
  and generated or vendored code is never in them (FR-017).
- It is cross-platform without a shell: the `justfile` runs PowerShell on Windows, so recipes with
  loops and globs would need two versions.
- One clang-tidy command per file lets Ninja run them in parallel and re-run only what changed.
- Everything goes through `just`, in `build-cli/`, like the existing `build` and `test` recipes.

**Alternatives considered**

- `CMAKE_CXX_CLANG_TIDY` (lint while compiling): slows every build and ties lint to the compiler
  run.
- `run-clang-tidy.py`: needs Python on every contributor's machine and its own file filter.
- Qt's generated `all_qmllint` target: it is what `lint-qml` wraps. It is not used directly
  because the recipe has to fail on warnings, which the settings file provides.

## R7. What lint needs from a build (SC-002, edge case "run before a build")

**Decision**: `CMAKE_EXPORT_COMPILE_COMMANDS` is turned on in `CMakeLists.txt`, and the `lint`
target depends on `appgav` and `gav_tests` being built.

**Rationale**: clang-tidy needs the compile commands, and both linters need files the build
generates (Qt's type registrations and resource lists). Making the dependency explicit means
`just lint` on a fresh checkout builds first rather than failing with missing-file errors. The
measurements above were taken against a built tree.

## R8. Where the automatic checks run (FR-019, FR-020, SC-005)

**Decision**: Both checks are added as steps to the existing Linux x64 leg of the `build` job in
`.github/workflows/build.yaml`:

- "Check formatting" right after CMake is configured and before the build, so a formatting
  failure is reported before the slow part. It cannot run earlier because the check is a CMake
  target.
- "Lint" after the build step.

Both are enabled in one pull request, after the code passes both checks (see the plan's delivery
order). Each step's failure message ends with the command to run locally (`just format` or
`just lint`).

**Rationale**

- That leg already installs Qt, restores the vcpkg cache, configures and builds, which is
  everything lint needs. A separate job would repeat all of it.
- The Linux x64 leg currently finishes in about 5.5 minutes and the Windows and macOS legs in
  about 9, so adding a few minutes to it does not lengthen the pull request's total time.
- **Estimate to verify**: the curated clang-tidy run took 99 s here with 8 jobs. On a 4-core
  runner it should take 3 to 4 minutes. This is an estimate, not a measurement; the first CI run
  of the lint step is the check against SC-005.
- clang-format and clang-tidy 23.1.0 have to be installed on the runner. They come from the
  builds the maintainer plans to publish (R1). If the checks are turned on before those exist,
  the fallback is the LLVM 23.1.0 release archive on GitHub, from which only the two binaries are
  unpacked and then cached; it is about 1.7 GB, so the download counts towards the 5 minutes on a
  run without the cache. LLVM's apt repository is not used because it cannot give an exact point
  release. T035 measures the time either way.

**Known side effect**: the matrix cancels its other legs when one fails, so a lint failure also
shows the other platforms as failed. That is how it already behaves for test failures.

## R9. Files that build on one platform only (FR-016)

**Decision**: `mediasession_windows.cpp` and `mediasession_macos.mm` are formatted like every
other file but are exempt from clang-tidy. The exemption and its reason are written next to the
file list in `support/lint.cmake`.

**Rationale**: They do not compile on the platform where lint runs, so there are no compile
commands for them there. Running clang-tidy on the Windows and macOS legs as well would add a
tool install and several minutes to the two slowest legs for two thin back-end files. The compiler
warnings on those platforms still apply to them.

**Alternative considered**: lint on all three platforms. Deferred; it can be added later without
changing anything decided here.

## R10. Suppressions and opt-outs (FR-008, FR-014, SC-007)

**Decision**

| What | Form |
|---|---|
| One C++ finding | `// NOLINT(check-name): reason` on the line, or `// NOLINTNEXTLINE(check-name): reason` above it |
| One QML finding | `// qmllint disable category` … `// qmllint enable category`, with the reason in a comment on the line above |
| C++ layout kept by hand | `// clang-format off: reason` … `// clang-format on` |
| QML layout kept by hand | `// qmlformat off` … `// qmlformat on`, with the reason in a comment on the line above |
| A check off for the whole project | A commented line in `.clang-tidy` or `.qmllint.ini` |

The two QML markers take the reason on the line above because the tools read everything after the
marker as part of it: `// qmlformat off: reason` is not recognised and the block gets reformatted
(checked), while `// clang-format off: reason` is.

A small check in the `lint` target fails if any of these markers in a first-party file lacks the
rule name or the reason.

**Rationale**: SC-007 asks for every suppression to name its rule and carry a reason; a machine
check keeps that true. These comments are the one place where the project's "default to no code
comments" rule has to give way, because the reason is the point.

## R11. Version pinning and mismatches (FR-021, FR-022, SC-006)

**Decision**

- The pinned version is `GAV_LLVM_VERSION` in `support/lint.cmake`.
- Each tool is the binary of that name found on `PATH`. The target compares the version it
  reports with the pinned one and stops on a mismatch with the version found and the version
  needed. If none is found, it stops with a message that says which version is needed and how to
  point at a binary.
- The cache variables `GAV_CLANG_FORMAT` and `GAV_CLANG_TIDY` name a binary to use instead. It
  gets the same version check.
- Every check prints the tool versions at the start of its output.

**Rationale**: A different clang-format version would silently reformat lines the pinned one
accepts. Because the binaries are taken from the machine, the version check is what enforces the
pin: it stops early instead of producing a confusing diff. qmlformat and qmllint come from the
pinned Qt, which the build already checks.

## R12. Landing the reformat (FR-006, FR-009, FR-024)

**Decision**

- Before the reformat, `.gitattributes` marks `*.cpp`, `*.h`, `*.mm` and `*.qml` as text with LF
  line endings. Both styles write LF, so without this a Windows checkout that converts line
  endings would see every file as misformatted.
- The reformat is two commits, one for C++ and one for QML, each made by running `just format`
  with nothing else in it. The tests run before and after.
- A `.git-blame-ignore-revs` file lists those two commits. GitHub reads it automatically; locally
  it is enabled with `git config blame.ignoreRevsFile .git-blame-ignore-revs`.
- The pull request is merged with a merge commit, as the project's pull requests already are, so
  the listed commit ids stay valid.
- The contributor documentation says how to carry a branch across it: rebase onto the commit
  before the reformat, run `just format`, then rebase onto `main` preferring the branch's side.

**Rationale**: Keeping formatting-only commits separate is what makes "nothing but formatting"
checkable, and it is what lets blame skip them.

## R13. Effect on the constitution

**Decision**: The Development Workflow rule "Code MUST match the surrounding style and idioms.
Default to no code comments." is amended, as part of this feature's last delivery step, to say
that layout is whatever `just format` produces, that `just lint` must pass, and that suppression
and opt-out markers with a reason are allowed comments. This is a clarification of an existing
rule plus one new obligation, so a MINOR bump.

**Rationale**: Once the checks gate merges, the written rule should point at them. Until that
amendment, nothing in this plan contradicts the constitution: the reformat keeps the dominant
style, and the tools are development-only.
