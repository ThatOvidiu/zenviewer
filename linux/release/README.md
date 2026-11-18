# Release build

The ordinary build, against the system's Qt (6.4 or newer), from the `release`
preset in `CMakePresets.json`: `-O3` and link-time optimization. It builds into
`build/` here, which git ignores:

```sh
./linux/release/build.sh  # same as: cmake --preset release && cmake --build --preset release
./linux/release/build/zenviewer path/to/image.jpg
sudo cmake --install linux/release/build
```

Installing puts the binary, `linux/zenviewer.desktop` and the icons in the
system folders (see `linux/linux.cmake`). Where the system's Qt is too old, use
[`../bundle`](../bundle/README.md) instead.
