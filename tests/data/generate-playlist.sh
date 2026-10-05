#!/usr/bin/env bash
set -euo pipefail

COUNT="${1:?usage: generate-playlist.sh <count> [output]}"
OUTPUT="${2:-/dev/stdout}"

awk -v count="$COUNT" 'BEGIN {
    print "#EXTM3U"
    for (i = 1; i <= count; i++) {
        group = sprintf("Group %02d", (i - 1) % 40 + 1)
        printf "#EXTINF:-1 tvg-id=\"channel%d.example\" tvg-logo=\"https://example.org/logos/%d.png\" group-title=\"%s\",Channel %04d\n", i, i, group, i
        printf "https://example.org/live/%d/index.m3u8\n", i
    }
}' > "$OUTPUT"
