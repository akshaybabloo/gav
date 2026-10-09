# Contract: Stream Addresses and Credentials

What GAV accepts as a stream address, and what it does with a user name and password inside one.
Covers FR-001 to FR-003, FR-015 to FR-025 and SC-003.

## Accepted addresses

| Way in | `http`, `https` | `rtsp`, `rtsps` | Anything else with a scheme |
|---|---|---|---|
| Open URL dialog | Yes | Yes (new) | Refused, with the message below |
| Command line argument | Yes | Yes (new) | Refused, with the message below on standard error |
| Starting GAV again while it runs | Yes | Yes (new) | Refused |
| Operating system "open address" request (media controls) | Yes | Yes (new) | Ignored |
| An entry in a playlist file | Yes | Yes (new) | Left out and counted as unsupported |
| An entry in a playlist downloaded from an address | Yes | Yes (new) | Left out and counted as unsupported |
| Drag and drop of files | Not affected | Not affected | Not affected |

Still `http` and `https` only: the address of a playlist to download, channel logos, HLS
playlists and their parts, and subtitle files.

Schemes are compared without regard to case. An address must have a host.

**Texts**

| Where | Text |
|---|---|
| Open URL dialog, description | "Enter the address of a video or audio file, a stream (.m3u8 or rtsp://) or a playlist (.m3u)." |
| Open URL dialog, hint under the field when the address is refused | "Enter an address starting with http://, https://, rtsp:// or rtsps://" |
| Command line, refused address | "Unsupported address (http, https, rtsp and rtsps streams can be opened): *display address*" |

## The display address

The address with the user name and password removed. Nothing else changes.

| Full address | Display address |
|---|---|
| `rtsp://viewer:s3cr3t@192.168.1.20:554/stream1` | `rtsp://192.168.1.20:554/stream1` |
| `rtsp://viewer@cam.local/live` | `rtsp://cam.local/live` |
| `rtsps://admin:p%40ss@cam.local/ch1?sub=1` | `rtsps://cam.local/ch1?sub=1` |
| `https://user:pw@example.org/a.m3u8` | `https://example.org/a.m3u8` |
| `rtsp://192.168.1.20/stream1` | unchanged |

## Where each form is used

| Place | Form |
|---|---|
| The connection to the source | Full |
| The playlist entry in memory, for as long as it is in the playlist | Full |
| Playlist row (second line), search and filter matching | Display |
| A title made from the address when the entry has none | Made from the display address |
| Window title, mini player title | Display |
| Statistics overlay, and what its copy button copies | Display |
| Snackbar and dialog messages | Display |
| Operating system now-playing data (title, address) | Display |
| Recent files (list and menu) | Display |
| Session playlist | Display |
| Log, at every level, including lines that come from Qt and FFmpeg | Display |
| A playlist file the user saves | The user's choice; see below |

These rules hold for `http` and `https` addresses with credentials as well (FR-022).

## Logging

Every line passes through one filter inside the logger before it is written, whichever part of
GAV or of its libraries produced it. The filter replaces any `scheme://user:password@` or
`scheme://user@` inside the text with `scheme://`. It applies to the main application and to the
probe subprocess. Text GAV prints outside the logger, such as the refused-address line on the
command line, goes through the same function.

## Saving a playlist

| Situation | Behaviour |
|---|---|
| No entry has a password | Saved as today, with no question |
| At least one entry has a password | A dialog asks: "This playlist contains passwords. Save them in the file?" with **Save with passwords**, **Save without passwords** and **Cancel** |
| Save without passwords | Every address is written as its display address |
| Save with passwords | A second dialog asks: "The passwords will be written to *file name* as plain text. Anyone who can read the file can use them. Save with passwords?" with **Save with passwords** and **Cancel**. Cancel is the default button |
| Save with passwords, confirmed in the second dialog | Every address is written in full |
| Cancel in either dialog, or either dialog dismissed | Nothing is written |
| Any save that did not go through both confirmations | Display addresses only. The writer never keeps credentials unless told to |
| The session playlist GAV writes by itself | Never asks; always display addresses |

Loading is unchanged apart from the accepted schemes: an entry's title, group, logo and other
attributes are read as for any stream entry.

## Opening a remembered address

A remembered address has no credentials. GAV opens it as it is. If the source asks for a
sign-in, the sign-in message of
[rtsp-connection.md](./rtsp-connection.md#messages) is shown, and its action opens the Open URL
dialog with the display address filled in so that the user can add the credentials.
