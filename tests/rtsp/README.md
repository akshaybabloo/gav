# RTSP test source

A local RTSP source for checking RTSP playback by hand or with headless runs. The scenarios that
use it are in `specs/004-rtsp-streams/quickstart.md`. Nothing here is built or shipped.

## What you need

- [MediaMTX](https://github.com/bluenviron/mediamtx/releases) 1.21.2: the release binary for your
  platform, or the container image `bluenviron/mediamtx:1.21.2`
- `ffmpeg`, to publish a test picture and tone
- `openssl`, once, for the certificate that secure RTSP needs
- `python3`, for the server that sends no media

## Start it

Run everything from this directory.

```text
openssl req -x509 -newkey rsa:2048 -nodes -keyout server.key -out server.crt -days 30 -subj "/CN=127.0.0.1"

mediamtx mediamtx.yml
```

With the container image instead of the binary:

```text
docker run --rm --network host -v "$PWD":/cfg -w /cfg bluenviron/mediamtx:1.21.2 /cfg/mediamtx.yml
```

`server.key` and `server.crt` are ignored by git.

Then publish, in a second terminal:

```text
ffmpeg -re -f lavfi -i testsrc=size=640x360:rate=25 -f lavfi -i sine=frequency=440 \
  -c:v libx264 -preset ultrafast -tune zerolatency -g 25 -pix_fmt yuv420p -c:a aac \
  -f rtsp -rtsp_transport tcp rtsp://127.0.0.1:8554/test
```

- Publish to `rtsp://127.0.0.1:8554/cam` in the same way for the path that needs a sign-in.
- Add `-vn` before `-f rtsp` for a stream without picture, or `-an` for one without sound.

For the split view's camera checks (`specs/005-split-view/quickstart.md`), publish to `wall1` to
`wall4` with the time drawn on the picture, so a live picture can be told from a paused one:

```text
for i in 1 2 3 4; do
  ffmpeg -re -f lavfi -i testsrc=size=640x360:rate=25 -f lavfi -i sine=frequency=$((300+i*100)) \
    -vf "drawtext=text='wall$i %{localtime\:%T}':x=20:y=20:fontsize=36:fontcolor=white:box=1:boxcolor=black" \
    -c:v libx264 -preset ultrafast -tune zerolatency -g 25 -pix_fmt yuv420p -c:a aac \
    -f rtsp -rtsp_transport tcp rtsp://127.0.0.1:8554/wall$i &
done
```

## Addresses

| What | Address |
|---|---|
| Open stream | `rtsp://127.0.0.1:8554/test` |
| The same, secure | `rtsps://127.0.0.1:8322/test` |
| Needs a sign-in | `rtsp://viewer:s3cr3tpw@127.0.0.1:8554/cam` |
| Four open streams for the split view | `rtsp://127.0.0.1:8554/wall1` to `wall4` |
| Sends no media | `rtsp://127.0.0.1:8555/s` |

## One transport only

Set `rtspTransports` in `mediamtx.yml` to `[tcp]` or `[udp]` and restart MediaMTX. With `[udp]`,
publish with `-rtsp_transport udp`.

## A server that sends no media

```text
python3 silent_server.py
```

It answers the RTSP handshake on `127.0.0.1:8555` and never sends anything, which is what a
client sees when a network drops UDP. It prints each `SETUP` with the transport the client asked
for, so a fallback from UDP to TCP shows up as a second `SETUP`.
