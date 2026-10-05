# GAV Constitution

## Core Principles

### I. Simple, Focused Player

GAV is a simple audio and video player. Every feature MUST directly serve playing, navigating,
or inspecting local or user-supplied media.

- Features MUST work offline. Network access is only allowed for user-initiated actions (opening a
  stream URL) and the opt-out update check.
- GAV MUST NOT require accounts, telemetry, or cloud services.
- User data (settings, history, playlists) MUST stay on the user's machine.
- New UI MUST NOT add permanent clutter to the main playback view. Secondary controls belong in
  menus, popups, or settings.

Rationale: keeping GAV small and predictable is what distinguishes it from heavier players.

### II. Responsive UI Through Isolation

The interface MUST stay responsive no matter what media is being processed.

- Decoding, probing, frame extraction, and file or network I/O that can take more than a frame's
  time MUST NOT run on the UI thread.
- Batch or untrusted-input media processing (e.g. collage generation) MUST run in an isolated
  subprocess with a timeout, so a crash or hang cannot take down the main application.
- Failures MUST be surfaced to the user as a clear message (snackbar or dialog). They MUST NOT
  cause silent hangs or crashes.

Rationale: media files are untrusted and codecs fail in surprising ways. Isolation keeps one bad
file from ruining the session.

### III. Cross-Platform Parity

GAV ships on Linux (x64 and arm64), Windows, and macOS, and MUST build and pass tests on every
platform in the CI matrix.

- A feature MUST behave the same on all platforms unless the OS lacks the underlying capability.
- Platform-specific code MUST be isolated behind a single interface, with a graceful no-op or
  hidden UI on platforms that don't support it.
- Packaging (DEB, RPM, TGZ, AppImage, DMG, NSIS, ZIP) MUST keep working after each change.

Rationale: users choose GAV as a consistent player across machines. Platform gaps are bugs, not
features.

### IV. Pinned, Patchable Dependencies

Every third-party dependency MUST be version-pinned and updatable independently of upstream
release cycles.

- C/C++ libraries MUST come from vcpkg via `vcpkg.json`, with the vcpkg baseline pinned.
- FFmpeg is pinned through GAV's own multimedia plugin so CVE fixes can be picked up without
  waiting for a Qt release. Changes MUST NOT reintroduce a dependency on Qt's bundled FFmpeg.
- A new dependency MUST be justified in the feature's plan, explaining why Qt, FFmpeg, or
  existing dependencies are insufficient.

Rationale: a media player parses hostile input, so the ability to patch quickly is a security
requirement.

### V. Tested Core Logic

Logic that can run without a GUI MUST have automated tests.

- Parsing, formatting, path handling, playlist or history manipulation, and similar non-visual
  logic MUST be covered by unit tests in `tests/` and run through `ctest`.
- Bug fixes in testable logic MUST include a regression test.
- UI behaviour that cannot reasonably be automated MUST have manual validation steps in the
  feature's `quickstart.md`.
- CI MUST be green on all platforms before a merge.

Rationale: most of GAV's risk lives in non-visual logic, where tests are cheap and effective.

## Technology Constraints

- Language: C++20. UI: Qt 6 with QML, in the `gavqml` module, with QML files kept at the
  repository root.
- Media: Qt Multimedia backed by FFmpeg (see Principle IV).
- Build: CMake 4.0+, vcpkg, Ninja. The CMake target is `appgav` and the output binary is `gav`.
- Logging: `spdlog`. User-visible errors go through the UI, not only the log.
- Persistence: user settings use Qt Settings. New persisted state MUST follow the same mechanism
  unless the plan justifies otherwise.
- Version is injected at configure time via `GAV_VERSION`. Do not hard-code it.

## Development Workflow

- Work is tracked as GitHub issues. Branches are named `<issue-number>-<short-slug>` and merged to
  `main` through pull requests.
- Each PR SHOULD deliver one independently usable increment (one user story or fix).
- Code MUST match the surrounding style and idioms. Default to no code comments. Prefer clear
  names and small functions instead.
- Dependency updates are handled by Renovate and MUST pass the full CI matrix like any other
  change.

## Governance

This constitution takes precedence over other development practices for GAV. Every `/speckit-plan`
MUST include a Constitution Check, and any violation MUST be recorded with justification in the
plan's Complexity Tracking table.

- Amendments: proposed by changing this file in a PR, with the Sync Impact Report updated to
  describe the change.
- Versioning: MAJOR for removing or redefining a principle; MINOR for adding a principle or section
  or materially expanding one; PATCH for clarifications and wording.
- Compliance: reviewers check PRs against these principles. `CLAUDE.md` holds runtime development
  guidance and MUST NOT contradict this document.

**Version**: 1.0.0 | **Ratified**: 2026-10-02 | **Last Amended**: 2026-10-02
