#!/bin/sh
# Builds ZenViewer as a Flatpak, against the KDE runtime's Qt.
# The result is linux/flatpak/build/zenviewer-<version>.flatpak, and the app is
# installed for this user. Needs flatpak, and Flathub as a remote.
set -e
id=io.github.zenviewer.ZenViewer
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root/linux/flatpak"
# The source to build, as a tar on stdout: the project's own files and folders,
# without what the builds produce. Named one by one, so that anything else lying
# in the project folder (sample images, installers, .git) stays out.
archive() {
    tar -C "$root" --exclude='./linux/*/build' --exclude=./windows/build -cf - \
        ./CMakeLists.txt ./CMakePresets.json ./LICENSE ./README.md ./src ./linux ./windows
}
if command -v flatpak-builder >/dev/null; then
    builder=flatpak-builder
else
    flatpak install --user --noninteractive flathub org.flatpak.Builder
    builder="flatpak run org.flatpak.Builder"
fi
rm -rf build/src
mkdir -p build/src
archive | tar -x -C build/src
$builder --user --force-clean --install --install-deps-from=flathub \
    --state-dir=build/state --repo=build/repo build/app "$id.yaml"
version=$(sed -n 's/^project(zenviewer VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")
flatpak build-bundle --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo \
    build/repo "build/zenviewer-$version.flatpak" "$id"
echo "Built linux/flatpak/build/zenviewer-$version.flatpak; run it with: flatpak run $id"
