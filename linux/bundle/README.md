# Bundle build

Builds ZenViewer against its own shared Qt 6.12, for distributions whose Qt is
too old (or missing). The result is a folder, like the Windows package: the
`zenviewer` binary, and a `lib` folder beside it with Qt's libraries, its plugins
(`lib/platforms`, `lib/imageformats`…) and the few other libraries that not every
system has. You need Podman or Docker, and a few GB of free disk space while it
builds:

```sh
linux/bundle/build.sh
./linux/bundle/build/out/zenviewer/zenviewer path/to/images
```

It builds in an Ubuntu 22.04 container, so the bundle runs on any x86-64
distribution with glibc 2.35 or newer: Ubuntu 22.04+, Debian 12+, Fedora 36+,
RHEL / Rocky / Alma 10, openSUSE Leap 16, and current rolling ones.

The first run compiles Qt, which takes a while, and later runs only rebuild
ZenViewer from the project folder as it is now. Everything stays in
`linux/bundle/build/`, which git ignores; delete that folder to start over. Next
to the folder, `out/` also gets `zenviewer-<version>-linux-x86_64.tar.gz` to
hand out; unpack it anywhere and run `zenviewer/zenviewer`.
The bundle's own `README.txt` (`bundle-readme.txt` here) tells whoever
unpacks it how to run it, add it to the app menu, and what the system needs.

## App menu and PATH

The bundle carries `zenviewer.desktop` and the icons (`icons/hicolor/…`). Its
`install.sh` adds ZenViewer to the app menu, to "Open with" for images, and puts
a `zenviewer` command on the PATH (a link to the binary, which still finds `lib/`
beside the real file). Run as a user, it installs for that user alone and needs no
root; run with `sudo`, it installs for everyone:

```sh
zenviewer/install.sh                    # ~/.local/share and ~/.local/bin
sudo zenviewer/install.sh               # /usr/local/share and /usr/local/bin
zenviewer/install.sh --uninstall        # take it out again (sudo for the second)
```

It leaves an existing `zenviewer` in that `bin` folder alone unless it is its own
link, so it doesn't replace a `cmake --install` of the release build, and it says
so when `~/.local/bin` isn't in the PATH. The entry and the command run the binary
where the folder is, so run `install.sh` again after moving the folder. Until then
the launcher hides the entry rather than showing a broken one.

## What the system needs

Everything ZenViewer needs is in `lib`, except what has to match the system: the
C library and C++ runtime, the X11 and Wayland client libraries, xkbcommon,
fontconfig and FreeType. A desktop has all of these, with one exception.

**`libxkbcommon-x11`** isn't installed everywhere: Xfce on Fedora and Arch,
Hyprland on Arch and GNOME on recent Fedora don't bring it in by themselves
(another Qt app often does). Without it ZenViewer can't start on X11, and Qt's
message blames the wrong library:

```
qt.qpa.plugin: From 6.5.0, xcb-cursor0 or libxcb-cursor0 is needed to load the Qt xcb platform plugin.
qt.qpa.plugin: Could not load the Qt platform plugin "xcb" in "" even though it was found.
```

Wayland sessions don't need it. To install it:

| Distribution | Command |
|---|---|
| Debian, Ubuntu | `sudo apt install libxkbcommon-x11-0` |
| Fedora, RHEL | `sudo dnf install libxkbcommon-x11` |
| Arch | `sudo pacman -S libxkbcommon-x11` |

It stays out of `lib` on purpose: it has to be the same release as the system's
`libxkbcommon`, or ZenViewer crashes on X11.

Tested on Ubuntu 22.04, Debian 12, Fedora 36 and 45, and Arch, on X11 and Wayland.

## Good to know

- **Finding `lib`:** the binary looks for its libraries in `lib` beside it (its
  RUNPATH is `$ORIGIN/lib`), and `main.cpp` points Qt at the plugins there, so the
  folder can live anywhere, with no `qt.conf` or launcher script.
- **Drawing:** Qt is built without OpenGL or D-Bus, so all drawing happens in
  software.
- **Updates:** rebuild to pick up fixes in Qt. Its version is in `container.sh`,
  and changing it means updating `SHA256SUMS` too.
- **Licensing:** Qt is LGPL, which a separate `lib` folder satisfies: anyone can
  swap in their own Qt 6.12 build. The bundle carries ZenViewer's licence,
  `THIRD-PARTY-NOTICES.txt` and a `licenses` folder, covering Qt, the libraries
  inside it and the Ubuntu libraries in `lib`. `notices.py` writes them on every
  build, from the Qt sources and the Ubuntu packages, so they follow the Qt
  version and whatever ends up in `lib`. When you publish a bundle, also publish
  the Qt source archives the notices list.
