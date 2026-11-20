# TinyEXIF

ZenViewer reads EXIF metadata (camera, lens, exposure, date, GPS) with
[TinyEXIF](https://github.com/cdcseacave/TinyEXIF) 1.2.1, commit `5e08e79`.

`TinyEXIF.h`, `TinyEXIF.cpp`, `LICENSE` and `LICENSE.easyexif` are copied
unchanged from that release. It is built into the app, with
`TINYEXIF_NO_XMP_SUPPORT` set, so the XMP parser and its tinyxml2 dependency
are left out.

TinyEXIF is under the MIT License (`LICENSE`); parts that come from easyexif
are also under the BSD-2-Clause licence (`LICENSE.easyexif`). Both texts ship
with every ZenViewer package.

To update: replace the four files with those of a newer release, and change
the version here and in the notices (`windows/notices/gen_notices.py`,
`windows/notices/THIRD-PARTY-NOTICES.txt` and `linux/bundle/notices.py`).
