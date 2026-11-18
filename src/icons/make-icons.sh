#!/bin/sh
# Regenerates the app icon set from the two SVGs in this folder.
# zenviewer.svg is the full design; zenviewer-small.svg has a thicker ring and
# no sun, so it stays readable at 16-32 px. Needs rsvg-convert and ImageMagick.
set -e
cd "$(dirname "$0")"
for size in 16 24 32; do rsvg-convert -w $size -h $size zenviewer-small.svg -o zenviewer-$size.png; done
for size in 48 64 128 256; do rsvg-convert -w $size -h $size zenviewer.svg -o zenviewer-$size.png; done
# Windows icon with all sizes, for the exe and the installer
convert zenviewer-16.png zenviewer-24.png zenviewer-32.png zenviewer-48.png zenviewer-64.png \
    zenviewer-128.png zenviewer-256.png zenviewer.ico
