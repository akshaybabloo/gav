# GAV Constitution

## Core Principles

### I. Simple, Focused Player

GAV is a simple audio and video player. Every feature MUST directly serve playing, navigating,
or inspecting local or user-supplied media.

- Features MUST work offline. Network access is only allowed for user-initiated actions and the
  opt-out update check. Opening a stream or remote playlist covers the requests needed to play it:
  its playlists, a download-speed check, and a subtitle track the user picks. Every such request
  MUST have a timeout and a size limit, and MUST stop when the user closes the media.
- Decorative images that a playlist points to, such as channel logos, MAY be downloaded only while
  a setting the user has turned on allows it. That setting MUST be off by default. These requests
  MUST be limited to entries the user is looking at, MUST have a timeout and a size limit, and MUST
  stop when the setting is turned off. Any cache of such images MUST be clearable by the user.
- GAV MUST NOT require accounts, telemetry, or cloud services.
- User data (settings, history, playlists) MUST stay on the user's machine.
- Anything that records what the user played (resume positions, recent files, the last playlist)
  MUST be off by default and MUST store the least identifying data that does the job.
- When media offers alternatives (video qualities, audio formats, audio and subtitle tracks),
  every one of them MUST stay selectable. An automatic default is fine. Hiding or merging choices
  is not.
- New UI MUST NOT add permanent clutter to the main playback view. Secondary controls belong in
  menus, popups, or settings.

Rationale: keeping GAV small and predictable is what distinguishes it from heavier players.

### II. Responsive UI Through Isolation

The interface MUST stay responsive no matter what media is being processed.

- Decoding, probing, frame extraction, and file or network I/O that can take more than a frame's
  time MUST NOT run on the UI thread.
- Batch or untrusted-input media processing (e.g. collage generation, subtitle and chapter
  probing) MUST run in an isolated subprocess with a timeout, so a crash or hang cannot take down
  the main application.
- Small still images (cover art, channel logos) MAY be decoded inside the main application, off
  the UI thread, provided the accepted formats are an explicit allow-list and both the input size
  and the pixel dimensions are limited. All other untrusted media decoding stays in a subprocess.
- Content fetched from the network is untrusted. It MUST NOT be able to make GAV open local files
  or address schemes GAV does not explicitly support. Text formats parsed in-process (playlists)
  MUST be parsed off the UI thread.
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
- FFmpeg is pinned through GAV's own multimedia plugin, and the copy linked into `gav` for
  probing, so CVE fixes can be picked up without waiting for a Qt release. Changes MUST NOT
  reintroduce a dependency on Qt's bundled FFmpeg, and packages MUST NOT ship it.
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
- Media: Qt Multimedia backed by FFmpeg (see Principle IV). Subtitles are rendered with libass.
- Build: CMake 4.0+, vcpkg, Ninja. The CMake target is `appgav` and the output binary is `gav`.
  The `justfile` wraps configure, build and test in a separate `build-cli/` directory.
- Logging: `spdlog`. User-visible errors go through the UI, not only the log. A normal run MUST
  NOT print routine progress output, including output from third-party libraries. That belongs
  behind `--verbose`.
- Persistence: preferences use Qt Settings. Playback history lives in
  `<AppDataLocation>/history.json`, the last-session playlist in `<AppDataLocation>/session.m3u8`,
  and disposable files in `<CacheLocation>`. New persisted state MUST use one of these unless the
  plan justifies otherwise.
- Version is injected at configure time via `GAV_VERSION`. Do not hard-code it.

## Development Workflow

- Work is tracked as GitHub issues. Branches are named `<issue-number>-<short-slug>` and merged to
  `main` through pull requests.
- Each PR SHOULD deliver one independently usable increment (one user story or fix).
- Code MUST match the surrounding style and idioms. Default to no code comments. Prefer clear
  names and small functions instead.
- When behaviour changes after its spec was written, the feature's `spec.md`, `research.md`,
  `tasks.md` and `quickstart.md` MUST be updated in the same change.
- Dependency updates are handled by Renovate and MUST pass the full CI matrix like any other
  change.

## Governance

This constitution takes precedence over other development practices for GAV. Every `/speckit-plan`
MUST include a Constitution Check, and any violation MUST be recorded with justification in the
plan's Complexity Tracking table.

- Amendments: proposed by changing this file in a PR that describes the change. The Sync Impact
  Report comment is scratch material for reviewing an amendment and MUST be removed before the
  file is committed.
- Versioning: MAJOR for removing or redefining a principle; MINOR for adding a principle or section
  or materially expanding one; PATCH for clarifications and wording.
- Compliance: reviewers check PRs against these principles. `CLAUDE.md` holds runtime development
  guidance and MUST NOT contradict this document.

**Version**: 1.2.0 | **Ratified**: 2026-10-02 | **Last Amended**: 2026-10-06
