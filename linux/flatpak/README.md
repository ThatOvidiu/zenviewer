# Flatpak build

Builds ZenViewer as a [Flatpak](https://flatpak.org/). Qt comes from the KDE
runtime (`org.kde.Platform` 6.11), which Flatpak installs once and shares between
apps, so nothing here compiles or carries Qt, and the same package runs on any
distribution that has Flatpak. You need `flatpak`, with Flathub as a remote:

```sh
linux/flatpak/build.sh
flatpak run io.github.zenviewer.ZenViewer path/to/images
```

The script builds the project folder as it is now (no git needed), installs the
app for your user, and writes `build/zenviewer-<version>.flatpak` to hand out
(about 220 kB). Whoever gets that file installs it with `flatpak install
zenviewer-<version>.flatpak`, which also fetches the runtime from Flathub if it
isn't there yet. The first build downloads the KDE SDK (about 2 GB) and, without
`flatpak-builder` on the system, the `org.flatpak.Builder` app. Everything else
stays in `build/`, which git ignores.

| File | What it is |
|---|---|
| `io.github.zenviewer.ZenViewer.yaml` | The manifest: runtime, permissions, and how to build |
| `io.github.zenviewer.ZenViewer.metainfo.xml` | Name, description and releases, as software centres show them |
| `build.sh` | Runs the build |

## Good to know

- **The id** `io.github.zenviewer.ZenViewer` is a placeholder. Flathub wants one
  from a domain or a code-hosting account you own (`io.github.<user>.ZenViewer`).
  It is in both file names, inside both files and in `build.sh`.
- **Permissions:** the app gets the whole file system (`--filesystem=host`). A
  viewer walks the folder of the image it was given and moves files to the trash,
  and the portal that hands a sandboxed app one chosen file can't offer that.
- **In the code:** two lines of `main.cpp` are for Flatpak. The window's app id
  follows `FLATPAK_ID`, so the desktop matches the window to the menu entry, and
  `XDG_DATA_HOME` is set back to the host's, so Delete reaches the real trash.
- **Formats:** the KDE runtime has plugins for more formats (AVIF, HEIC, JXL…),
  but ZenViewer opens the same ones as everywhere else.
- **Flathub:** publishing there needs a public repository (the manifest's source
  becomes a git URL and tag instead of `build/src`), a homepage and screenshots
  in the metainfo, and their review.
