#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

FONT="${GAV_FIXTURE_FONT:-/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/signs.ass" <<'EOF'
[Script Info]
ScriptType: v4.00+
PlayResX: 320
PlayResY: 240

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Sign,DejaVu Serif,24,&H0000FFFF,&H000000FF,&H00000000,&H00000000,-1,0,0,0,100,100,0,0,1,2,0,8,10,10,10,1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
Dialogue: 0,0:00:01.00,0:00:09.00,Sign,,0,0,0,,{\pos(160,40)}Signs track
Dialogue: 0,0:00:12.00,0:00:18.00,Sign,,0,0,0,,{\pos(160,200)\frz15}Rotated
EOF

cat > "$TMP/chapters.txt" <<'EOF'
;FFMETADATA1
[CHAPTER]
TIMEBASE=1/1000
START=0
END=10000
title=One
[CHAPTER]
TIMEBASE=1/1000
START=10000
END=20000
title=Two
[CHAPTER]
TIMEBASE=1/1000
START=20000
END=30000
title=Three
EOF

ffmpeg -hide_banner -loglevel error -y \
  -f lavfi -i "testsrc=size=320x240:rate=25:duration=30" \
  -f lavfi -i "sine=frequency=440:duration=30" \
  -f lavfi -i "sine=frequency=880:duration=30" \
  -i "$TMP/signs.ass" \
  -i "$TMP/chapters.txt" \
  -map 0:v -map 1:a -map 2:a -map 3:s -map_metadata 4 -map_chapters 4 \
  -c:v libx264 -preset ultrafast -crf 40 -g 25 -pix_fmt yuv420p \
  -c:a aac -b:a 32k -c:s ass \
  -metadata:s:a:0 language=eng -metadata:s:a:1 language=jpn \
  -metadata:s:s:0 language=eng -metadata:s:s:0 title=Signs \
  -attach "$FONT" -metadata:s:t mimetype=application/x-truetype-font \
  multi.mkv

cat > multi.en.srt <<'EOF'
1
00:00:01,000 --> 00:00:04,000
Hello from the English sidecar.

2
00:00:05,000 --> 00:00:08,000
Second English line.
EOF

python3 - <<'EOF'
text = "1\r\n00:00:01,000 --> 00:00:04,000\r\nÉté, déjà vu, garçon.\r\n\r\n2\r\n00:00:05,000 --> 00:00:08,000\r\nÇa va très bien, merci.\r\n"
open("multi.fr.srt", "wb").write(text.encode("cp1252"))
EOF

cat > styled.ass <<'EOF'
[Script Info]
ScriptType: v4.00+
PlayResX: 1280
PlayResY: 720
WrapStyle: 0

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Default,DejaVu Sans,48,&H00FFFFFF,&H000000FF,&H00000000,&H80000000,0,0,0,0,100,100,0,0,1,3,1,2,40,40,40,1
Style: Sign,DejaVu Serif,56,&H0000FFFF,&H000000FF,&H00000000,&H00000000,-1,0,0,0,100,100,0,0,1,2,0,7,0,0,0,1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
Dialogue: 0,0:00:03.00,0:00:08.00,Sign,,0,0,0,,{\pos(100,100)}Positioned sign
Dialogue: 0,0:00:10.00,0:00:15.00,Sign,,0,0,0,,{\pos(800,400)\frz30}Rotated sign
Dialogue: 0,0:00:18.00,0:00:23.00,Default,,0,0,0,karaoke,{\k100}Ka{\k100}ra{\k100}o{\k100}ke{\k100}!
EOF

ffmpeg -hide_banner -loglevel error -y \
  -f lavfi -i "color=c=0x202020:size=1280x720:rate=25:duration=25" \
  -c:v libx264 -preset ultrafast -crf 40 -pix_fmt yuv420p \
  styled.mp4

cat > playlist-relative.m3u8 <<'EOF'
#EXTM3U
#EXTINF:30,Multi
multi.mkv
#EXTINF:25,Styled
./styled.mp4
#EXTINF:10,Missing
missing.mp4
EOF

head -c 64 multi.mkv > corrupt.mkv

ffmpeg -hide_banner -loglevel error -y \
  -f lavfi -i "sine=frequency=440:duration=5" \
  -c:a libmp3lame -b:a 64k \
  tone.mp3

cat > mixed.m3u8 <<'EOF'
#EXTM3U
#EXTINF:30,Multi
multi.mkv
#EXTINF:5,Tone
tone.mp3
#EXTINF:600 group-title="Films",Film
https://test-streams.mux.dev/x36xhzz/x36xhzz.m3u8
#EXTINF:-1 tvg-id="News.uk" tvg-logo="https://example.org/news.png" group-title="News",News Channel
https://example.org/live/news.m3u8
#EXTINF:10,Missing
missing.mp4
#EXTINF:30,Multi again
multi.mkv
#EXTGRP:Samples
#EXTINF:25,Styled
styled.mp4
#EXTINF:-1,Ungrouped stream
https://example.org/live/other.m3u8
EOF
