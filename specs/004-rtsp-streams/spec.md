# Feature Specification: RTSP Streams over TCP or UDP

**Feature Branch**: `004-rtsp-streams`

**Created**: 2026-10-09

**Status**: Draft

**Input**: User description: "I want to stream RTSP on UDP or TCP, more information in gh 179" (issue #179, "Support RTSP streams")

## Background

GAV opens network streams only from `http://` and `https://` addresses. An `rtsp://` address, the
kind that IP cameras, video recorders and many media servers publish, is refused in the Open URL
dialog and on the command line, and is dropped when a playlist is loaded.

Issue #179 asks for these streams to be playable and to be handled as live. RTSP can carry its
video over two transports. TCP gets through routers and firewalls and loses nothing, at the cost
of a little delay. UDP has the lowest delay but is often blocked, and lost packets show as picture
damage. Which one works depends on the user's network, so GAV has to find the one that works and
the user needs to be able to choose.

Camera addresses usually carry a user name and password inside the address itself. GAV shows and
remembers addresses in several places, so this feature also has to keep those credentials out of
them.

The people this serves are users who watch cameras and other live sources on their own network,
and users whose channel playlists contain RTSP entries.

## Clarifications

### Session 2026-10-09

- Q: When an RTSP stream cannot start with the transport chosen in Settings, what should GAV do? → A: The setting has three values. "Automatic" is the default and tries UDP first, then TCP. Choosing TCP or UDP forces that one, with no fallback.
- Q: After GAV restarts, should a camera that was opened with a user name and password in its address play again without the user typing the credentials again? → A: No. GAV stores no passwords anywhere; remembered addresses leave the credentials out, and the user opens the camera again with its full address.
- Q: When an RTSP stream that was playing stops because the camera or the network dropped, should GAV try to reconnect by itself? → A: Yes, a few times: up to 3 attempts over about 30 seconds while showing "Reconnecting…", then it gives up with a message. Closing the stream or opening something else cancels it.
- Q: When the user saves the playlist to a file of their choosing and some entries have a user name and password in their address, what should be written to that file? → A: GAV asks at save time whether to save with the passwords or without them, and writes what the user picks.
- Q: Is one question enough before passwords are written to a playlist file? → A: No. Passwords are written only with the user's express consent, asked twice: choosing "with passwords" is followed by a second confirmation. No build may write them without that consent.
- Q: May a password reach the log through any path? → A: No. Every line GAV writes to its log is covered, including lines GAV writes directly and not only those that come from the libraries it uses.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Watch an RTSP stream (Priority: P1)

A user has the address of a camera or media server. They enter it in the Open URL dialog, or pass
it on the command line, and the stream plays as a live stream: it has a LIVE badge and no seek
position, exactly like the live streams GAV already plays.

**Why this priority**: This is the feature. Nothing else in it has value until an RTSP address
plays.

**Independent Test**: Start a test RTSP source on the local network, open its address from the
Open URL dialog and again from the command line, and confirm that picture and sound play and the
stream is shown as live.

**Acceptance Scenarios**:

1. **Given** GAV is open, **When** the user enters a reachable `rtsp://` address in the Open URL
   dialog and confirms, **Then** the stream starts playing and is added to the playlist as a
   stream entry.
2. **Given** GAV is not running, **When** the user starts it with an `rtsp://` address as its
   argument, **Then** it opens and plays that stream.
3. **Given** GAV is already running, **When** the user starts it again with an `rtsp://` address,
   **Then** the running window opens that stream.
4. **Given** an RTSP stream is playing, **When** the user looks at the controls, **Then** the LIVE
   badge is shown, seeking, go to time, frame stepping, A-B repeat and the seek preview are
   unavailable, and volume, mute, pause, full screen, the mini player and the audio track menu
   work as usual.
5. **Given** an RTSP stream was played and closed, **When** the user opens it again, **Then** no
   resume prompt appears and no resume position was stored for it.
6. **Given** the user enters an address that cannot be reached, **When** they confirm, **Then** a
   message says the stream could not be opened, within 15 seconds, and the window stays
   responsive while it waits.
7. **Given** the user enters an address with a scheme GAV does not play, **When** they confirm,
   **Then** the message lists the kinds of address that are accepted, including RTSP.
8. **Given** a secure RTSP address (`rtsps://`), **When** the user opens it, **Then** it plays in
   the same way.
9. **Given** an RTSP stream is playing, **When** the source or the network drops for a few
   seconds and comes back, **Then** GAV shows that it is reconnecting and the stream carries on
   by itself.
10. **Given** an RTSP stream is playing, **When** the source goes away and stays away, **Then**
    GAV tries to reconnect up to 3 times over about 30 seconds, then stops and says the stream
    was lost.
11. **Given** GAV is reconnecting, **When** the user closes the stream or opens something else,
    **Then** the reconnecting stops at once.

---

### User Story 2 - Choose TCP or UDP (Priority: P2)

Without touching any setting, a user's stream plays over UDP when the network allows it and over
TCP when it does not. A user who wants one transport only, because the picture is damaged over
UDP or because they want the lowest delay and nothing else, opens Settings and picks TCP or UDP,
and from then on streams use exactly that.

**Why this priority**: It is what the request names. One fixed transport leaves some users with a
stream that cannot play at all on their network, and others with more delay than they need.

**Independent Test**: With a test source that accepts both transports, play it with the setting
on Automatic and confirm the transport in use is UDP; block UDP, open it again and confirm it
plays over TCP. Then force each transport in turn and confirm it is the one in use, and that a
forced UDP attempt with UDP blocked fails with a message that points to the setting.

**Acceptance Scenarios**:

1. **Given** a fresh installation, **When** the user opens Settings, **Then** the RTSP transport
   is set to Automatic, and TCP and UDP are the other two choices.
2. **Given** the transport is Automatic and the network lets UDP through, **When** the user opens
   an RTSP stream, **Then** it is carried over UDP.
3. **Given** the transport is Automatic and UDP is blocked or refused by the source, **When** the
   user opens an RTSP stream, **Then** GAV tries UDP, gives up on it within 5 seconds, and plays
   the stream over TCP without the user doing anything.
4. **Given** a source needed TCP once in this session, **When** the user opens the same source
   again with the transport on Automatic, **Then** it starts over TCP straight away, without the
   wait for UDP.
5. **Given** the user sets the transport to TCP or to UDP, **When** they open an RTSP stream,
   **Then** it is carried over that transport only, with no restart of GAV needed.
6. **Given** an RTSP stream is playing, **When** the user opens the playback statistics overlay,
   **Then** it shows which transport the stream is using.
7. **Given** the transport is forced to UDP and the network blocks it, **When** the user opens a
   stream, **Then** a message says the stream could not be opened over UDP and that Automatic or
   TCP can be chosen in Settings. GAV does not try TCP.
8. **Given** the transport is Automatic and the stream cannot be opened over either transport,
   **When** the user opens it, **Then** one message says the stream could not be opened.
9. **Given** a stream is playing over one transport, **When** the user changes the setting,
   **Then** the stream that is playing is not interrupted, and the new choice applies from the
   next stream opened.
10. **Given** any transport setting, **When** the user opens a secure RTSP address, **Then** it
    plays over TCP, because a secure stream cannot be carried the other way, and the statistics
    overlay says so.

---

### User Story 3 - Credentials stay private (Priority: P3)

A user opens a camera whose address contains a user name and a password. The stream plays, and
the password is not shown on screen, not sent to the operating system's media controls, not
written to the log and not saved in anything GAV remembers.

**Why this priority**: Without it, playing a camera once would leave its password in plain text
in several files and on screen. It is listed third only because it can be built and tested
separately; it has to be in the same release as Story 1.

**Independent Test**: Turn on recent files and session restore, start GAV with verbose logging,
play an address that contains a user name and a password, close GAV, and search the history
file, the saved session playlist and the log for the password. It must not be found. While the
stream plays, check each place that shows the address.

**Acceptance Scenarios**:

1. **Given** an address with a user name and a password, **When** the user opens it, **Then** the
   credentials are used to sign in to the source and the stream plays.
2. **Given** that stream is playing, **When** the user looks at the window title, the playlist
   row, the statistics overlay, any message GAV shows and the operating system's now-playing
   display, **Then** the address appears without the user name and password wherever it appears.
3. **Given** recent files and session restore are turned on, **When** the user plays that stream
   and closes GAV, **Then** the history file and the saved session playlist contain the address
   without the user name and password.
4. **Given** GAV runs with verbose logging, **When** the user plays that stream, **Then** the
   password appears nowhere in the log.
5. **Given** the stream is in the playlist during a session, **When** the user plays it again in
   the same session, **Then** it plays without the credentials being entered again.
6. **Given** a remembered address whose credentials were left out, **When** the user opens it
   from recent files or a restored session, **Then** GAV tries it as it is, and if the source
   asks for a sign-in, a message says the address needs a user name and password and how to open
   it with them.
7. **Given** an address with wrong credentials, **When** the user opens it, **Then** the message
   says that signing in failed, as distinct from the source being unreachable.

---

### User Story 4 - RTSP entries in playlists (Priority: P4)

A user loads a channel playlist that has RTSP entries among its HTTP ones. The RTSP entries are
kept, shown as streams, and play when chosen.

**Why this priority**: It is the smaller of the two ways RTSP addresses reach GAV, and it only
works once the first three stories do.

**Independent Test**: Load a playlist file with HTTP, RTSP and unsupported entries, confirm the
RTSP entries are listed and play, and that the unsupported ones are still left out.

**Acceptance Scenarios**:

1. **Given** a playlist file with `rtsp://` and `rtsps://` entries, **When** the user loads it,
   **Then** those entries appear in the playlist with their titles, groups and logos, marked as
   streams.
2. **Given** a playlist downloaded from an address, **When** it contains RTSP entries, **Then**
   they are kept in the same way.
3. **Given** a playlist with entries of other kinds GAV does not play, **When** the user loads
   it, **Then** those are left out as before and RTSP entries are unaffected.
4. **Given** an RTSP entry in the playlist, **When** next or previous reaches it, **Then** it
   plays according to the transport setting.
5. **Given** a playlist in which at least one entry has a password in its address, **When** the
   user saves it to a file of their choosing, **Then** GAV asks whether to save with the
   passwords or without them.
6. **Given** the user chooses to save without the passwords, **When** the file is written,
   **Then** no address in it has a user name or password.
7. **Given** the user chooses to save with the passwords, **When** they confirm that choice,
   **Then** GAV asks a second time, saying that the passwords will be written to the file as
   plain text, and writes them only if the user confirms again.
8. **Given** either question is showing, **When** the user cancels it, **Then** nothing is saved.
9. **Given** a playlist with no password in any address, **When** the user saves it, **Then** it
   is saved as before, with no question.

---

### Edge Cases

- The source accepts only one of the two transports, and the setting is forced to the other one.
- The setting is Automatic and the source accepts only TCP: the UDP attempt is refused at once
  rather than timing out.
- UDP is allowed by the source but blocked on the way: the connection is made, yet no picture
  ever arrives.
- The stream stops part-way: the camera is switched off, the network drops, or the source closes
  the session. GAV reconnects a limited number of times (FR-027a).
- The source comes back during reconnecting but now refuses the sign-in, or now offers a different
  picture size or different tracks.
- The stream drops while it is paused, or while the window is minimised or in the mini player.
- The address has a user name but no password, or the password contains characters that have a
  special meaning in an address.
- The source needs credentials and the address has none.
- The stream has no sound, or no picture, or more than one audio track.
- The stream changes its picture size while playing.
- The source offers a recording with a fixed length rather than a live feed. It is shown as
  live, and when it reaches its end it stops like any other entry instead of being reconnected.
- A playlist fetched from the network contains an RTSP entry that points at a device on the
  user's own network.
- The address names a port, or leaves it out.
- The user opens several RTSP streams one after another quickly, or closes one while it is still
  connecting.
- A secure RTSP address is opened on a platform or network where a secure connection cannot be
  made.
- The user pauses a live RTSP stream for a long time and then resumes.

## Requirements *(mandatory)*

### Functional Requirements

**Opening**

- **FR-001**: Users MUST be able to open `rtsp://` and `rtsps://` addresses from the Open URL
  dialog, from the command line, and by starting GAV again while it is already running.
- **FR-002**: The Open URL dialog's hint and the message for an unsupported address MUST name
  RTSP among the accepted kinds of address.
- **FR-003**: Addresses of any other kind that GAV does not play MUST still be refused with that
  message.
- **FR-004**: Opening a stream MUST NOT freeze the window. While a stream is connecting, the user
  MUST see that it is connecting and MUST be able to cancel by closing it or opening something
  else.

**Live behaviour**

- **FR-005**: An RTSP stream MUST be presented as live: it shows the LIVE badge, and seeking, go
  to time, frame stepping, A-B repeat and the seek preview are unavailable.
- **FR-006**: A resume position MUST NOT be stored or offered for an RTSP stream.
- **FR-007**: GAV MUST NOT look for alternative qualities or for subtitle tracks for an RTSP
  stream. Audio tracks the stream itself carries MUST stay selectable.
- **FR-008**: Pause, volume, mute, full screen, the mini player, brightness and contrast, zoom and
  the operating system's media keys MUST work for an RTSP stream as they do for other live
  streams.

**Transport**

- **FR-009**: Settings MUST offer a choice of RTSP transport with three values: Automatic, TCP
  and UDP. The default MUST be Automatic.
- **FR-010**: The chosen transport MUST apply to every RTSP stream opened after the choice is
  made, however it is opened, without restarting GAV. A stream that is already playing MUST NOT
  be interrupted by the change.
- **FR-011**: The playback statistics overlay MUST show the transport an RTSP stream is using.
- **FR-012**: With Automatic, GAV MUST try UDP first. If the source refuses UDP, or no picture or
  sound arrives within 5 seconds, it MUST open the stream over TCP instead, without asking.
- **FR-012a**: With Automatic, once a source has needed TCP, GAV MUST open that source over TCP
  directly for the rest of the session. This is not remembered after GAV closes.
- **FR-012b**: With TCP or UDP chosen, GAV MUST use that transport only. When the stream cannot
  be opened with it, the message MUST name the transport and say that Automatic or the other one
  can be chosen in Settings.
- **FR-013**: A secure RTSP address MUST be carried over TCP whatever the setting says, and the
  statistics overlay MUST show that.
- **FR-014**: When a connection is made but no picture or sound arrives, GAV MUST stop waiting
  and, after any fallback FR-012 allows, report the failure within the same overall time limit
  as for an address that cannot be reached.

**Credentials**

- **FR-015**: An address MAY contain a user name and password. GAV MUST use them to sign in to
  the source and for nothing else.
- **FR-016**: Wherever GAV shows an address or a title derived from it (window title, playlist
  row, recent files, statistics overlay, messages, the operating system's now-playing display),
  the user name and password MUST be left out.
- **FR-017**: Whatever GAV saves by itself (recent files, the session playlist, any other history)
  MUST hold the address without the user name and password. GAV MUST NOT keep a password in any
  form once it closes, including in the operating system's password store.
- **FR-018**: The password MUST NOT be written to the log at any verbosity, by any path: lines
  GAV writes itself, and output that comes from the media libraries GAV uses.
- **FR-019**: For as long as an entry stays in the playlist during a session, it MUST keep its
  credentials so that it can be played again without re-entering them.
- **FR-020**: When a source refuses the credentials, or asks for some and none were given, the
  message MUST say that signing in failed or is needed, as distinct from the source being
  unreachable, and MUST say that the address can be opened again with a user name and password.
- **FR-021**: When the user saves the playlist to a file they name and at least one entry has a
  password in its address, GAV MUST ask, before writing, whether to save with the passwords or
  without them. "Without" leaves the user name and password out of every address in the file.
  "With" MUST be followed by a second question that says the passwords will be written as plain
  text, and they MUST be written only if the user confirms that one too. Cancelling either
  question MUST save nothing. A playlist with no password in any address MUST be saved without a
  question.
- **FR-021a**: GAV MUST NOT write a password to any file without that consent. This MUST hold in
  every build that accepts addresses with passwords in a playlist.
- **FR-022**: Requirements FR-016 to FR-018 MUST hold for every network address GAV accepts, not
  only RTSP ones.

**Playlists**

- **FR-023**: When a playlist is loaded from a file or from an address, its `rtsp://` and
  `rtsps://` entries MUST be kept, with their title, group and logo, and shown as streams.
- **FR-024**: Entries with addresses of kinds GAV does not play MUST still be left out.
- **FR-025**: An RTSP entry MUST behave like any other stream entry for search, filtering,
  sorting, grouping, selection, removal, and next and previous.

**Failures**

- **FR-026**: An address that cannot be reached, a refused sign-in, an unsupported transport and a
  stream that stops part-way and cannot be reconnected MUST each produce a clear message. They MUST NOT leave the window
  frozen or the player stuck in a loading state.
- **FR-027**: Every attempt to open an RTSP stream MUST give up after a fixed time, and MUST stop
  at once when the user closes the stream or opens something else.

- **FR-027a**: When an RTSP stream that was playing stops without the user stopping it, GAV MUST
  try to open it again by itself, up to 3 times over about 30 seconds, and MUST show that it is
  reconnecting while it does. If an attempt succeeds, playback MUST carry on as live with no
  further action from the user. If none does, GAV MUST stop trying and say that the stream was
  lost.
- **FR-027b**: Reconnecting MUST use the transport the stream was using and the credentials the
  entry holds. It MUST stop at once when the user closes the stream or opens something else, and
  MUST NOT start for a stream that never began playing or that the source refused to sign in.

**Platforms**

- **FR-028**: RTSP playback over both transports MUST work in every package GAV ships, on Linux,
  Windows and macOS.

### Key Entities

- **RTSP address**: a network address of a live source. It has a kind (plain or secure), a host,
  an optional port, a path, and optionally a user name and password.
- **Display address**: the same address with the user name and password left out. It is the only
  form that is shown or saved by GAV itself.
- **Transport setting**: the user's choice of Automatic, TCP or UDP for RTSP streams. One value
  for the whole application, kept with the other preferences.
- **Transport in use**: TCP or UDP, for one stream that is playing. With Automatic it is whichever
  of the two worked.
- **Stream entry**: a playlist entry for a network stream. An RTSP entry is one of these and is
  always treated as live.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a local network, picture and sound from an RTSP source start within 5 seconds of
  the user confirming its address, over TCP and over UDP. With the transport on Automatic and UDP
  blocked, they start within 10 seconds the first time and within 5 seconds after that.
- **SC-002**: An address that cannot be reached, or a forced transport that is blocked, produces a
  message within 15 seconds whatever the setting, and the window responds to input throughout.
- **SC-003**: After a stream with a user name and a password in its address has been played with
  every history option turned on and verbose logging, the password is found zero times in the
  files GAV wrote, in its log, on screen and in the operating system's now-playing display.
- **SC-004**: With default settings, a source that works over only one of the two transports
  plays without the user changing anything. A user can also force a transport and have a stream
  playing over it in under 30 seconds, without restarting GAV.
- **SC-005**: Of the RTSP entries in a loaded playlist, 100 % are listed, and each one plays when
  its source is reachable.
- **SC-006**: An RTSP stream on a stable local network plays for 30 minutes without stopping,
  freezing or losing the sound.
- **SC-007**: The same test source plays over both transports, forced and through Automatic, in
  the packages for Linux, Windows and macOS.
- **SC-008**: Opening HTTP and HTTPS streams and local files behaves as it did before this
  feature.
- **SC-009**: A playing RTSP stream whose source is away for up to 10 seconds is playing again
  within 10 seconds of the source returning, without the user doing anything. A source that
  stays away produces the "stream lost" message within 45 seconds.

## Assumptions

- "Stream RTSP on UDP or TCP" means watching RTSP sources in GAV with a choice of transport. GAV
  does not publish or re-send streams.
- The transport choice is one application-wide setting. A choice per address, and other
  transports such as multicast or tunnelling through HTTP, are out of scope.
- Automatic is the default so that a stream plays on any network without the user knowing what a
  transport is. It tries UDP first for the lowest delay; the cost is a wait of up to 5 seconds,
  once per source and session, on networks that block UDP.
- Forcing TCP or UDP means exactly that one. A forced choice never falls back, so the setting can
  be used to find out what a network allows.
- Every RTSP stream is treated as live, including a source that offers a recording with a fixed
  length. Seeking within such a recording is out of scope.
- Credentials are given only inside the address. There is no sign-in dialog and GAV does not
  store passwords anywhere by itself, so a remembered camera has to be opened again with its full address
  after a restart. Storing passwords in the operating system's keychain is out of scope.
- A playlist the user saves to a file of their choosing may keep credentials, because that file
  is theirs and a camera list without them would not play, but only after the user has said so
  twice at save time. What GAV saves by itself always leaves them out.
- Reconnecting applies to RTSP streams only and only to a stream that was already playing. How
  HTTP and HTTPS streams behave when they stop is unchanged. Retrying without limit, for a camera
  left on screen all day, is out of scope.
- Recording a stream, taking a collage from one, and camera controls such as pan and zoom are out
  of scope. Frame capture works as it does for other live streams.
- The other kinds of address mentioned in the issue (`rtmp://`, `mmsh://`, `srt://`) stay
  unsupported in this feature.
- Stories 1 to 3 are released together. Story 3 is separate for building and testing, not for
  shipping.
- Testing needs an RTSP source. A local test server stands in for a real camera, and each
  platform's package is checked against it.
