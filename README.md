# ZenViewer

A lightweight image viewer with a media-player style interface: the image
fills the window and all controls are an auto-hiding on-screen display (OSD).
Built on Qt 6 Widgets with custom-painted vector icons, so it looks the same
on Linux and Windows. File dialogs stay native.

## Project layout

| Path | What it is |
|---|---|
| `src/` | The app's code, the same on every platform |
| `third_party/` | Others' code built into the app: [TinyEXIF](third_party/TinyEXIF/README.md), which reads the EXIF details |
| `CMakeLists.txt`, `CMakePresets.json` | The build, shared by all platforms; it includes the platform's own `.cmake` file |
| `linux/` | Linux: `linux.cmake` and the `.desktop` file, shared by its three builds: `release/` (the CMake preset, against the system's Qt), `bundle/` (a folder with its own Qt, to hand out) and `flatpak/` (a Flatpak, on the KDE runtime's Qt). Each keeps what it produces in its own `build/` |
| `windows/` | Windows: `windows.cmake`, `build.bat`, `installer.bat` + `installer.iss`, `notices/` with the licences that ship with the app, and `build/` for what the Windows build produces |

Git ignores the `build/` folders.

## Linux

Requires CMake ≥ 3.21, a C++20 compiler and Qt ≥ 6.4 (Widgets).

```sh
cmake --preset release && cmake --build --preset release
./linux/release/build/zenviewer path/to/image.jpg
sudo cmake --install linux/release/build
```

`linux/release/build.sh` runs the first line. The `release` preset builds with
`-O3` and link-time optimization into `linux/release/build/`; the install puts
`zenviewer` in `/usr/local/bin` with its app menu entry and icons.
`-DZENVIEWER_LTO=OFF` turns link-time optimization off.

Extra formats (WebP, TIFF, ICNS…) come from the `qt6-imageformats` package and
SVG from `qt6-svg`. HEIC, AVIF, JXL, RAW and PDF aren't supported, even if a plugin
for them is installed.

For a folder with its own shared Qt that runs where Qt 6.4 isn't available, see
[`linux/bundle`](linux/bundle/README.md); building it needs Podman or Docker.
For a Flatpak, see [`linux/flatpak`](linux/flatpak/README.md).

## Windows

### What you need

1. **Visual Studio Build Tools 2022 or 2026** (or Visual Studio Community) with the
   **Desktop development with C++** workload. It brings the MSVC compiler, the
   Windows SDK, CMake and Ninja.
2. **Qt 6.5 or newer** from the Qt Online Installer, with these components under
   *Qt → Qt 6.12.0*: **MSVC 2022 64-bit**, and under *Additional Libraries*
   **Qt Image Formats** (for WebP, TIFF…).

### Build

Run `windows\build.bat` (double-click it or run it from a Command Prompt). It
finds Visual Studio, builds the `windows` preset and puts `zenviewer.exe` in
`windows\build\`, with the Qt and Microsoft runtime DLLs and Qt's plugins in its
`dll` folder (a private assembly, so Windows finds them there; no `qt.conf`). That
folder is the whole app, apart from its `scripts` folder: zip it without `scripts`, or
copy it, to any 64-bit Windows 10 or 11 PC.
If Qt isn't in `C:\Qt\6.12.0\msvc2022_64`, run `set QT_DIR=<your Qt folder>`
first. CMake's generated build scripts go to `windows\build\scripts`, which is
always safe to delete; `windows\build.bat clean` starts over.

Without the script, from an *x64 Native Tools Command Prompt for VS*:

```bat
cmake --preset windows -DCMAKE_PREFIX_PATH=C:\Qt\6.12.0\msvc2022_64
cmake --build --preset windows
cmake --install windows\build\scripts
```

Qt Creator and Visual Studio (*File → Open → Folder*) also pick up the
`windows` preset.

### Licences

`windows\build` also gets `LICENSE.txt`, `THIRD-PARTY-NOTICES.txt` and a `licenses` folder
covering Qt (LGPL v3) and the libraries inside it. When you publish a build, also
publish the Qt source archives it lists next to it, since the LGPL asks for the
source of the Qt you ship. `windows/notices/gen_notices.py` regenerates the
notices from the Qt sources when you move to another Qt version.

## Using it

Press **H** (or F1) in the app, or run `zenviewer -h`, for the full list of keys
and mouse actions.
The essentials:

| Input | Action |
|---|---|
| Move mouse | Show title and arrows (they fade out after 1.9 s) |
| Mouse at the left / bottom edge | Show the icon bar / thumbnail strip, until the mouse leaves that edge |
| Enter | Show or hide the icon bar and the strip from the keyboard |
| ← / → , mouse Back / Forward | Previous / next image (the keys also show the thumbnail strip for a moment, slideshow or not) |
| P, F5 | Start / stop slideshow (Esc also stops it) |
| Space | Pause / resume a running slideshow (a big icon flashes; paused = two blinking bars) |
| `[` / `]`, S | Slide interval, slideshow settings (Done, S or Esc closes them) |
| Wheel | Previous / next image, like ← / → |
| Ctrl+Wheel, + / − | Zoom |
| Double-click, F | Full screen |
| Middle-click | Fit / actual size |
| Right-click, Menu key | A menu with every command and its key. Its File menu copies, moves or deletes the image, and the raw file shot with it (same name, a camera raw extension such as NEF, CR3, ARW or DNG): the image, both, or the raw alone |
| 0, 1 | Fit; actual size (1 again fits) |
| Delete ×2 | Move to trash |
| E | EXIF details: camera, lens, exposure, date and place, where the image has them (← / → move to the other images with it open; E, Esc or Enter closes it) |
| Ctrl+P | Print: a print preview, from which the image goes to a printer or into a PDF file |
| I, T, M | Info panel (leaves the controls alone; in a window, it and the title shrink with the image), thumbnail strip on / off, show / hide the icon bar for now (the left edge always brings it back) |
| L | Loop: the last image leads to the first and back |
| ↑ / ↓, Enter | Select an icon in the bar, press it (Esc leaves the bar) |
| Q, X | Quit, after a Yes / No confirmation (default No) |

EXIF details come from JPEG, PNG, WebP and TIFF files.

## License

ZenViewer's own code is under the [MIT License](LICENSE). It uses
[Qt](https://www.qt.io/), which is under the LGPL, and includes
[TinyEXIF](https://github.com/cdcseacave/TinyEXIF) (MIT, with parts under BSD-2-Clause). The Linux bundle and the
Windows package contain Qt, so they carry its licence and the notices for the
software inside it; see [`linux/bundle`](linux/bundle/README.md#good-to-know).
