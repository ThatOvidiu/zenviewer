#!/bin/sh
# Builds zenviewer against the system's Qt (6.4 or newer), from the release
# preset: -O3 and link-time optimization.
# The result is linux/release/build/zenviewer. Reruns rebuild only what changed.
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
cmake --preset release
cmake --build --preset release
echo "Built linux/release/build/zenviewer; install it with: sudo cmake --install linux/release/build"
