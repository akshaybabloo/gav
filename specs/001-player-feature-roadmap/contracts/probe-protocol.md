# Contract: Probe Subprocess Protocol

The interface between the GAV UI process and the media probe subprocess (research R3). Both sides
are the same `gav` binary. Breaking changes need `protocol` to be bumped.

## Invocation

```text
GAV_SUBPROCESS=probe gav --probe <path> --fonts-dir <dir> [--subtitle-file <path>]... [--events <streamIndex|all|none>]
```

| Argument | Meaning |
|----------|---------|
| `--probe <path>` | The media file to probe (local path only). Required. |
| `--subtitle-file <path>` | An external subtitle file to parse. Can be repeated. With `--events none`, only the files' headers are emitted. |
| `--events` | Which embedded subtitle streams to emit events for. Defaults to `all`. |
| `--fonts-dir <dir>` | Directory the probe writes embedded fonts into. Required with `--probe`. |

- The UI starts one probe per opened local file, plus one short-lived probe per external subtitle
  file loaded later.
- The UI kills the probe when another file is opened or the app quits.
- stdout carries the protocol only. Logs go to stderr, and the parent forwards them to `spdlog` at
  debug level.

## Framing

UTF-8 JSON Lines: one object per line, `\n`-terminated. Every object has a `type` field. Readers
must ignore unknown `type` values and unknown fields.

## Messages (child → parent), in order

1. `hello` (exactly once, first)

   ```json
   {"type":"hello","protocol":1}
   ```

2. `media` (once, for `--probe`)

   ```json
   {"type":"media","durationMs":5400000,
    "chapters":[{"startMs":0,"endMs":300000,"title":"Opening"}],
    "subtitleStreams":[{"stream":3,"codec":"ass","language":"eng","title":"Signs","default":false,"forced":true}],
    "fonts":[{"name":"Fancy.ttf","path":"/tmp/gav-probe-1234/fonts/Fancy.ttf"}]}
   ```

   - `subtitleStreams` lists text-based codecs only.
   - Fonts are written by the probe to a private temporary directory (`--fonts-dir`, created by
     the parent) and sent as file paths, never inline, so no message line carries bulk data.
     The parent loads them off the UI thread and deletes the directory when the file closes.

3. `header` (once per subtitle source that will emit events)

   ```json
   {"type":"header","source":"embedded:3","assHeader":"[Script Info]\n..."}
   {"type":"header","source":"external:/abs/movie.en.srt","assHeader":"...","encoding":"WINDOWS-1251"}
   ```

4. `event` (many, in roughly increasing file order, not strictly sorted)

   ```json
   {"type":"event","source":"embedded:3","startMs":61200,"durationMs":2300,"ass":"0,0,Default,,0,0,0,,Hello"}
   ```

   - `ass` is the dialogue line in Matroska `ReadOrder,Layer,Style,…,Text` form, ready for
     `ass_process_chunk`.

5. `end` (once per source, after its last event)

   ```json
   {"type":"end","source":"embedded:3"}
   ```

6. `progress` (at least every 2 s while reading, even when no events are found)

   ```json
   {"type":"progress","bytes":104857600}
   ```

7. `warning` (zero or more, at any point)

   ```json
   {"type":"warning","code":"font-write-failed","detail":"Fancy.ttf"}
   ```

8. `error` (at most once, last; the process then exits with a non-zero code)

   ```json
   {"type":"error","code":"open-failed","detail":"Invalid data found when processing input"}
   ```

   - Error codes: `open-failed`, `no-streams`, `decode-failed`, `encoding-failed`, `internal`.

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | Everything was emitted and `end` was sent for every source |
| 1 | An `error` message was emitted |
| other / signal | Crash. The parent treats every source still `Streaming` as `Failed` |

## Parent obligations

- Read stdout without blocking (`QProcess::readyReadStandardOutput`). Lines are small (no inline
  font data), so parsing on the receiving thread costs O(line length) and stays well under a frame.
- Treat all message contents as untrusted: validate types and ranges, and drop events with
  negative times.
- Watchdogs (Constitution Principle II): if `hello` hasn't arrived within 5 s, or no message of
  any kind (including `progress`) has arrived for 30 s, kill the probe and treat it as a crash.
  There is no limit on total run time, because large files take a while to stream, but a hung
  probe is always caught by the inactivity timeout.

## Test hooks

Unit tests cover the protocol line parser (`ProbeMessage::parse`). Golden-file
tests run the probe against fixtures in `tests/data/` and compare the output with stored `.jsonl`
expectations.
