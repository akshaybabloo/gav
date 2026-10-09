# Feature Specification: Code Linting and Formatting

**Feature Branch**: `003-code-lint-format`

**Created**: 2026-10-07

**Status**: Draft

**Input**: User description: "plan for https://github.com/akshaybabloo/gav/issues/195" (issue #195, "Code linting and formatting": "Use clang tidy and lint")

## Background

GAV has no shared formatting or linting rules. How a file is laid out depends on the editor of
whoever last touched it: the C++ sources already use two different indent widths, and nothing
checks either language before a change is merged. Likely defects and style drift are caught only
when a reviewer happens to notice them.

Issue #195 asks for automated linting and formatting. This feature gives the project one agreed
style and one agreed set of lint rules for its two languages (C++ and QML), brings the existing
code in line with them, and checks every change against them.

The people this serves are contributors (who write changes), the maintainer (who reviews and
merges them) and automated reviewers.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Format a change with one command (Priority: P1)

A contributor has edited some C++ and QML files. Before committing they run one command, and the
files come out in the project's style. They do not have to know the style rules or configure
their editor.

**Why this priority**: Formatting is the cheapest part to adopt, removes the most noise from
reviews, and has to land before linting so that later fixes are not mixed with layout changes.

**Independent Test**: On a fresh checkout, run the formatting check and confirm it reports nothing
to change. Then misformat one C++ file and one QML file, run the format command, and confirm both
are restored to exactly their committed content.

**Acceptance Scenarios**:

1. **Given** a fresh checkout, **When** the contributor runs the formatting check, **Then** it
   reports that every first-party source file already matches the style and exits successfully.
2. **Given** a contributor has changed the layout of a C++ file and a QML file, **When** they run
   the format command, **Then** both files are rewritten in the project style and nothing else in
   the working tree changes.
3. **Given** misformatted files, **When** the contributor runs the formatting check, **Then** it
   names each file that would change, changes nothing, and exits with a failure status.
4. **Given** the style rules live in the repository, **When** a contributor opens the project in
   an editor that supports them, **Then** the editor's own "format" action produces the same
   result as the command.
5. **Given** the existing code was reformatted, **When** someone looks at the history of a line,
   **Then** the bulk reformatting change can be skipped so the last meaningful change is shown.

---

### User Story 2 - Catch likely defects before review (Priority: P2)

A contributor runs one command and gets a list of likely defects, risky constructs and
inefficiencies in the C++ and QML code, each with the file, the line, the rule that fired and what
to do about it.

**Why this priority**: This is what the issue names first, and it is where real bugs are found.
It follows formatting because its fixes touch code, and it needs a clean baseline to be useful.

**Independent Test**: On a fresh checkout, run the lint command and confirm it reports no
findings. Then add one known problem to a C++ file and one to a QML file, run it again, and
confirm each is reported with its file, line and rule.

**Acceptance Scenarios**:

1. **Given** a fresh checkout that has been built once, **When** the contributor runs the lint
   command, **Then** it reports no findings and exits successfully.
2. **Given** a change that introduces a problem covered by the rule set, **When** the contributor
   runs the lint command, **Then** the finding is listed with file, line, rule name and message,
   and the command exits with a failure status.
3. **Given** a finding that is intentional or a false positive, **When** the contributor
   suppresses it, **Then** the suppression names the rule and gives a reason, applies to that
   place only, and the lint command passes again.
4. **Given** the rule set lives in the repository, **When** a contributor's editor supports it,
   **Then** the same findings appear in the editor while they type.

---

### User Story 3 - Every change is checked automatically (Priority: P3)

The maintainer opens a pull request. Formatting and lint checks run on it without anyone asking,
and the pull request cannot be merged while either fails.

**Why this priority**: Without enforcement the rules decay. It comes last because it depends on
the code already passing both checks.

**Independent Test**: Open a pull request that contains one formatting violation and one lint
finding and confirm the checks fail and say what is wrong. Fix both and confirm the checks pass.

**Acceptance Scenarios**:

1. **Given** a pull request whose files all match the style and raise no lint findings, **When**
   the checks run, **Then** they pass.
2. **Given** a pull request with a formatting violation, **When** the checks run, **Then** the
   formatting check fails, names the file, and states the command that fixes it.
3. **Given** a pull request with a lint finding, **When** the checks run, **Then** the lint check
   fails and shows the file, line, rule and message.
4. **Given** the checks have run, **When** the maintainer looks at how long the pull request took
   to validate, **Then** the two checks have added no more than five minutes.

---

### Edge Cases

- Generated code, build output, downloaded dependencies and the vendored package recipes are not
  first-party code and must be left alone by both tools.
- Source files that only build on one operating system (the Windows and macOS media-key
  back-ends) cannot be analysed on the others. They must still be formatted everywhere, and each
  must either be linted on the platform where it builds or be listed as exempt with the reason.
- Two contributors, or a contributor and the automated check, use different versions of a tool
  and get different results for the same file.
- A contributor does not have the tools installed.
- The one-time reformatting touches most files, so branches that were open before it will
  conflict with it.
- Reformatting or an automatic lint fix changes what the program does.
- A lint rule produces many findings in existing code that are not worth fixing.
- Text that must stay exactly as written, such as the icon code escapes in QML strings, is
  rewritten by a formatter.
- A file is deliberately laid out by hand (a table of constants, for example) and the formatter
  makes it worse.
- The lint command is run before the project has been built, when the information it needs does
  not exist yet.

## Requirements *(mandatory)*

### Functional Requirements

**Style and formatting**

- **FR-001**: The project MUST define one formatting style for C++ and one for QML, stored in the
  repository where editors and command-line tools find them without extra setup.
- **FR-002**: The chosen styles MUST follow what most of the existing code already does, so that
  the one-time reformatting changes as little as possible. They are not an occasion to introduce
  a new look.
- **FR-003**: Contributors MUST be able to format every first-party source file with one
  documented command.
- **FR-004**: Contributors MUST be able to check formatting with one documented command that
  changes no files, names every file that does not match, and reports failure when any does.
- **FR-005**: At the end of this feature every first-party C++ and QML file MUST match its style.
- **FR-006**: Formatting MUST NOT change behaviour. The reformatting of existing code MUST be made
  as changes that contain nothing but formatting, and the automated tests MUST pass before and
  after.
- **FR-007**: Formatting MUST leave string contents untouched, including the icon code escapes in
  QML.
- **FR-008**: A file or region that has to keep a hand-made layout MUST be able to opt out, with a
  stated reason next to the opt-out.
- **FR-009**: The bulk reformatting changes MUST be recorded in the repository so that history
  tools can skip them.

**Linting**

- **FR-010**: The project MUST define a lint rule set for C++ and one for QML, stored in the
  repository, each listing the rules that are on rather than relying on tool defaults.
- **FR-011**: The C++ rule set MUST cover likely defects, risky or deprecated constructs, and
  avoidable inefficiencies. The QML rule set MUST cover unresolved names and types, unused
  imports, and deprecated or incorrect use of the UI framework.
- **FR-012**: Contributors MUST be able to run all lint checks with one documented command that
  reports each finding with its file, line, rule name and message, and reports failure when there
  is any.
- **FR-013**: At the end of this feature the lint command MUST report no findings on the existing
  code. Each finding is either fixed or suppressed where it occurs.
- **FR-014**: A suppression MUST apply to one place, name the rule it silences, and give a reason.
  Turning a rule off for the whole project MUST be recorded with its reason in the rule set.
- **FR-015**: Lint fixes that change code MUST be separate from formatting changes, MUST keep the
  automated tests passing, and MUST NOT be applied automatically by the checks that run on pull
  requests.
- **FR-016**: Every first-party source file MUST be covered by lint on at least one platform, or
  be listed as exempt with a reason.

**Scope**

- **FR-017**: Both tools MUST cover the application sources, the unit tests and the QML files, and
  MUST exclude generated code, build output, downloaded dependencies and vendored package recipes.
- **FR-018**: Build scripts, workflow definitions, shell scripts and documentation are out of
  scope for this feature.

**Enforcement and reproducibility**

- **FR-019**: The formatting check and the lint check MUST run automatically on every pull request
  and on every change to the main branch, and a failure of either MUST block the merge.
- **FR-020**: When an automatic check fails, its output MUST show what failed (file, and for lint
  the line, rule and message) and state the command a contributor runs locally to reproduce or
  fix it.
- **FR-021**: The versions of the formatting and lint tools MUST be fixed, be the same for the
  automatic checks and the documented local setup, and be shown in the checks' output.
- **FR-022**: Running a check with a different tool version than the fixed one MUST produce a
  clear message rather than a silent difference in results.
- **FR-023**: The tools are for development only. They MUST NOT become part of the shipped
  application or add anything to what it needs at run time.

**Documentation**

- **FR-024**: The contributor documentation MUST describe how to install the tools, the format,
  format-check and lint commands, how to suppress a finding, and how to rebase a branch across
  the reformatting change.

### Key Entities

- **Style definition**: the formatting rules for one language, kept in the repository.
- **Rule set**: the list of lint rules that are on for one language, with any project-wide
  exclusions and their reasons.
- **Finding**: one reported problem: file, line, rule name, message.
- **Suppression**: a marker at one place in the code that silences one rule, with a reason.
- **Reformatting change**: a commit that contains only formatting, listed so history tools can
  skip it.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a fresh checkout, the formatting check and the lint check both report zero
  findings.
- **SC-002**: A contributor can format the whole code base with one command in under 30 seconds,
  and lint it with one command in under 5 minutes on a typical development machine once the
  project has been built.
- **SC-003**: Of ten deliberately introduced problems (five formatting, five lint, spread across
  C++ and QML), the automatic checks report all ten, each with its file and, for lint, its line
  and rule.
- **SC-004**: The automated test suite gives the same results before and after the existing code
  is reformatted and its lint findings are fixed, on every platform the project builds for.
- **SC-005**: The automatic checks add no more than 5 minutes to the time a pull request takes to
  validate.
- **SC-006**: Formatting the same file with the documented tool versions gives byte-identical
  output on a contributor's machine and in the automatic check.
- **SC-007**: Every suppression in the code base names its rule and carries a reason.

## Assumptions

- "Lint" in the issue means static analysis for both languages. The issue names the C++ analyser
  to use; for QML the linter and formatter that come with the UI framework are the natural
  counterparts, and for C++ formatting the analyser's companion formatter. Tool choice is settled
  in planning.
- Formatting is part of the request, as the issue title says, and applies to C++ and QML alike.
- The existing code is brought into line in this feature, rather than only checking lines changed
  from now on. The code base is small enough (about 11,000 lines of C++ and 8,000 of QML) for this
  to be one piece of work, and a whole-repository check is simpler to reason about than a
  per-change one.
- The rule sets start with rules whose findings can all be resolved in this feature. Stricter
  rules can be added later, one at a time.
- A failed check blocks the merge, in line with the project rule that the automated checks must
  pass before a merge.
- No git hooks are installed for contributors. Running the commands before committing is a
  convention; the automatic checks are the gate.
- The automatic checks run on one platform for formatting. Lint runs where the code builds, which
  may mean more than one platform for the platform-specific files.
- The reformatting lands when no other long-lived branches are open, so that rebasing across it
  is rare.
- The project's written style rule ("match the surrounding style") is updated to point at the
  style definitions once they exist. If that needs a constitution amendment, it is made as part of
  this feature.
