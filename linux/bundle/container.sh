#!/bin/bash
# Runs inside Ubuntu 22.04, with the recipe at /recipe and linux/bundle/build at /w.
# Each finished stage leaves a stamp, so a rerun continues where the last one stopped.
set -e
jobs=$(nproc)
qt=6.12.0
qt_url=https://download.qt.io/official_releases/qt/${qt%.*}/$qt/submodules
mkdir -p /w/logs /w/src /w/stamp /w/out /tmp/b

fail() {
    echo "FAILED: $1, see linux/bundle/build/logs/$2.log" >&2
    tail -20 "/w/logs/$2.log" >&2
    exit 1
}

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -qq -y --no-install-recommends >/dev/null \
    ca-certificates wget xz-utils build-essential g++-12 ninja-build perl python3 python3-pip pkg-config patchelf \
    libfontconfig1-dev libfreetype-dev libxkbcommon-dev libxkbcommon-x11-dev libwayland-dev wayland-protocols \
    libcups2-dev libx11-dev libx11-xcb-dev libxcb1-dev libxcb-cursor-dev libxcb-icccm4-dev libxcb-image0-dev \
    libxcb-keysyms1-dev libxcb-randr0-dev libxcb-render0-dev libxcb-render-util0-dev libxcb-shape0-dev \
    libxcb-shm0-dev libxcb-sync-dev libxcb-util-dev libxcb-xfixes0-dev libxcb-xkb-dev libxcb-xinput-dev
# Qt 6.12 needs CMake 3.25, newer than Ubuntu 22.04's; GCC 12 is the newest that
# Ubuntu 22.04's own libstdc++ covers, so the binaries need nothing newer than it
pip3 install -q --disable-pip-version-check cmake==3.31.6
export CC=gcc-12 CXX=g++-12

fetch() {
    file=$(basename "$1")
    [ -f "/w/src/$file" ] || wget -q -O "/w/src/$file" "$1"
    grep " $file\$" /recipe/SHA256SUMS | (cd /w/src && sha256sum -c --status -) || {
        echo "FAILED: checksum of $file doesn't match linux/bundle/SHA256SUMS" >&2
        exit 1
    }
}

# Qt, with everything ZenViewer doesn't use turned off. Image and text libraries
# are Qt's own copies, so the bundle doesn't depend on the system's versions;
# fontconfig and FreeType come from the system, to match its font setup, and so
# does CUPS, for printing. The stamp holds the options: change them and qtbase,
# and the modules built on it, are built again.
qtbase_options="-release -prefix /w/qt -nomake examples -nomake tests \
    -no-dbus -no-icu -no-glib -no-openssl -no-opengl \
    -no-feature-journald -no-feature-sql -no-feature-testlib -no-feature-concurrent \
    -no-feature-sessionmanager -no-feature-vulkan -no-feature-libinput -no-feature-tslib \
    -no-feature-mtdev -no-feature-libudev -no-feature-linuxfb -no-feature-vnc -no-feature-eglfs \
    -no-feature-network -no-feature-xml -no-zstd \
    -fontconfig -system-freetype -qt-harfbuzz -qt-libpng -qt-libjpeg -qt-pcre -qt-zlib -qt-doubleconversion \
    -cups -xcb -bundled-xcb-xinput -feature-wayland"
if [ "$(cat /w/stamp/qtbase 2>/dev/null)" != "$qtbase_options" ]; then
    rm -f /w/stamp/qtsvg /w/stamp/qtimageformats
    echo "== qtbase (the long part)"
    fetch $qt_url/qtbase-everywhere-src-$qt.tar.xz
    rm -rf /tmp/b/qtbase /tmp/b/qtbase-everywhere-src-$qt /w/qt
    mkdir /tmp/b/qtbase
    tar xf /w/src/qtbase-everywhere-src-$qt.tar.xz -C /tmp/b
    (cd /tmp/b/qtbase &&
        ../qtbase-everywhere-src-$qt/configure $qtbase_options &&
        cmake --build . --parallel "$jobs" &&
        cmake --install .) >/w/logs/qtbase.log 2>&1 || fail qtbase qtbase
    echo "$qtbase_options" >/w/stamp/qtbase
fi
qt_module() { # name, options
    [ -f "/w/stamp/$1" ] && return 0
    echo "== $1"
    fetch "$qt_url/$1-everywhere-src-$qt.tar.xz"
    rm -rf "/tmp/b/$1" "/tmp/b/$1-everywhere-src-$qt"
    mkdir "/tmp/b/$1"
    tar xf "/w/src/$1-everywhere-src-$qt.tar.xz" -C /tmp/b
    (cd "/tmp/b/$1" && /w/qt/bin/qt-configure-module "../$1-everywhere-src-$qt" $2 &&
        cmake --build . --parallel "$jobs" && cmake --install .) >"/w/logs/$1.log" 2>&1 || fail "$1" "$1"
    touch "/w/stamp/$1"
}
qt_module qtsvg
qt_module qtimageformats "-qt-webp -qt-tiff -no-jasper -no-mng"

# ZenViewer
echo "== zenviewer"
rm -rf /tmp/z
mkdir -p /tmp/z/src
tar xf /w/src/zenviewer.tar -C /tmp/z/src
(/w/qt/bin/qt-cmake -S /tmp/z/src -B /tmp/z/o -G Ninja -DCMAKE_BUILD_TYPE=Release &&
    cmake --build /tmp/z/o --parallel "$jobs") >/w/logs/zenviewer.log 2>&1 || fail zenviewer zenviewer

# The bundle: zenviewer, and lib/ with the libraries it needs and Qt's plugins
# (lib/platforms, lib/imageformats…), which main.cpp tells Qt to look for there.
app=/tmp/z/zenviewer
lib=$app/lib
mkdir -p "$lib"
cp /tmp/z/o/zenviewer "$app/"
for dir in /w/qt/plugins/*/; do
    case "$dir" in */generic/) continue ;; esac # evdev input, for systems without a display server
    cp -r "$dir" "$lib/"
done

# Libraries every Linux desktop has, and that must match the system (C library,
# C++ runtime, X11 and Wayland client libraries, fonts, graphics drivers).
# libxkbcommon and libxkbcommon-x11 share internals, so both come from the system:
# a bundled -x11 with the system's newer libxkbcommon crashes on X11.
system_lib() {
    case "$1" in
        ld-linux*|libc.so*|libm.so*|libdl.so*|libpthread.so*|librt.so*|libresolv.so*|libutil.so*| \
        libstdc++.so*|libgcc_s.so*| \
        libX11.so*|libX11-xcb.so*|libxcb.so.1|libXau.so*|libXdmcp.so*|libXext.so*|libXrender.so*| \
        libwayland-*|libxkbcommon*|libGL*|libEGL*|libOpenGL*|libdrm.so*|libgbm.so*| \
        libcups.so*| \
        libfontconfig.so*|libfreetype.so*|libexpat.so*|libz.so*|libpng16.so*|libbrotli*|libbz2.so*| \
        libffi.so*|libbsd.so*|libmd.so*|libuuid.so*) return 0 ;;
    esac
    return 1
}
find_lib() {
    if [ -e "/w/qt/lib/$1" ]; then
        echo "/w/qt/lib/$1"
    else
        ldconfig -p | awk -v n="$1" '$1 == n && /x86-64/ { print $NF; exit }'
    fi
}
# Copies what $1 needs into lib/, and what that needs in turn
add_needs() {
    local name path
    for name in $(objdump -p "$1" | awk '$1 == "NEEDED" { print $2 }'); do
        system_lib "$name" && continue
        [ -e "$lib/$name" ] && continue
        path=$(find_lib "$name")
        [ -n "$path" ] || { echo "FAILED: $name, needed by $1, isn't in the container" >&2; exit 1; }
        cp -L "$path" "$lib/$name"
        case "$path" in /w/qt/*) ;; *) echo "$path" >>/tmp/z/system-libs ;; esac # for the notices
        add_needs "$lib/$name"
    done
}
add_needs "$app/zenviewer"
for plugin in "$lib"/*/*.so; do
    add_needs "$plugin"
done

# Each file finds the others relative to itself, wherever the folder is
patchelf --set-rpath '$ORIGIN/lib' "$app/zenviewer"
for file in "$lib"/*.so*; do
    patchelf --set-rpath '$ORIGIN' "$file"
done
for file in "$lib"/*/*.so; do
    patchelf --set-rpath '$ORIGIN/..' "$file"
done
strip --strip-unneeded "$app/zenviewer" "$lib"/*.so* "$lib"/*/*.so

# App menu entry: the .desktop file and icons, which install.sh adds for the user.
# No MNG plugin in the bundle, so the entry doesn't offer to open MNG files.
sed 's#image/x-mng;##' /tmp/z/src/linux/zenviewer.desktop >"$app/zenviewer.desktop"
for size in 16 24 32 48 64 128 256; do
    mkdir -p "$app/icons/hicolor/${size}x$size/apps"
    cp "/tmp/z/src/src/icons/zenviewer-$size.png" "$app/icons/hicolor/${size}x$size/apps/zenviewer.png"
done
mkdir -p "$app/icons/hicolor/scalable/apps"
cp /tmp/z/src/src/icons/zenviewer.svg "$app/icons/hicolor/scalable/apps/"
install -m 755 /recipe/install.sh "$app/"
install -m 644 /recipe/bundle-readme.txt "$app/README.txt" # for whoever unpacks the tarball

# Licences: ZenViewer's, and THIRD-PARTY-NOTICES.txt with licenses/ for Qt (LGPL;
# the lib folder can be replaced with another Qt 6.12 build), the code inside Qt
# and the Ubuntu libraries in lib/. The notices come from the Qt sources.
cp /tmp/z/src/LICENSE "$app/LICENSE.txt"
for module in qtbase qtsvg qtimageformats; do
    [ -d "/tmp/b/$module-everywhere-src-$qt" ] || tar xf "/w/src/$module-everywhere-src-$qt.tar.xz" -C /tmp/b
done
QT_VERSION=$qt python3 /recipe/notices.py /tmp/b /tmp/z/system-libs "$app"
cp /tmp/z/src/third_party/TinyEXIF/LICENSE "$app/licenses/TinyEXIF-LICENSE.txt"
cp /tmp/z/src/third_party/TinyEXIF/LICENSE.easyexif "$app/licenses/TinyEXIF-easyexif-LICENSE.txt"

# Check: nothing the bundle needs is missing, and it starts
if LD_LIBRARY_PATH= ldd "$app/zenviewer" "$lib"/*.so* "$lib"/*/*.so | grep 'not found'; then
    echo "FAILED: libraries missing from the bundle (above)" >&2
    exit 1
fi
version=$(env -i QT_QPA_PLATFORM=offscreen "$app/zenviewer" --version | awk '{ print $NF }')
echo "zenviewer $version"

rm -rf /w/out/zenviewer
cp -a "$app" /w/out/zenviewer
tar -C /w/out -czf "/w/out/zenviewer-$version-linux-x86_64.tar.gz" zenviewer
[ -z "$HOST_IDS" ] || chown -R "$HOST_IDS" /w
echo "Done: linux/bundle/build/out/zenviewer and zenviewer-$version-linux-x86_64.tar.gz"
