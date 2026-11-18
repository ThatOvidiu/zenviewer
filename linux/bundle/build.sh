#!/bin/sh
# Builds zenviewer against a shared Qt in an Ubuntu 22.04 container, with Qt and
# the libraries most systems lack in a lib folder beside it.
# The result is linux/bundle/build/out/zenviewer/. Reruns reuse what is already built.
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
# The source to build, as a tar on stdout: the project's own files and folders,
# without what the builds produce. Named one by one, so that anything else lying
# in the project folder (sample images, installers, .git) stays out.
archive() {
    tar -C "$root" --exclude='./linux/*/build' --exclude=./windows/build -cf - \
        ./CMakeLists.txt ./CMakePresets.json ./LICENSE ./README.md ./src ./linux ./windows
}
if command -v podman >/dev/null; then
    engine=podman
    ids=
elif command -v docker >/dev/null; then
    engine=docker
    ids=$(id -u):$(id -g) # Docker runs as root, so hand the files back afterwards
else
    echo "This needs podman or docker." >&2
    exit 1
fi
mkdir -p linux/bundle/build/src
archive >linux/bundle/build/src/zenviewer.tar
exec "$engine" run --rm --init \
    -e "HOST_IDS=$ids" \
    -v "$root/linux/bundle:/recipe:ro,Z" \
    -v "$root/linux/bundle/build:/w:Z" \
    docker.io/library/ubuntu:22.04 bash /recipe/container.sh
