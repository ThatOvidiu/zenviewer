#!/usr/bin/env python3
"""Usage: gen_notices.py <folder with the unpacked Qt sources> <output folder>
Builds THIRD-PARTY-NOTICES.txt and licenses/ for the Windows ZenViewer package
from the qt_attribution.json files of the Qt sources it ships."""
import hashlib, json, os, re, shutil, sys

src, out = sys.argv[1], sys.argv[2]
QT = "6.12.0"
modules = [f"qtbase-everywhere-src-{QT}", f"qtsvg-everywhere-src-{QT}", f"qtimageformats-everywhere-src-{QT}"]
# Code that is not in the Windows DLLs and plugins ZenViewer ships
skip = re.compile(r"/(tests|examples|doc|tools|cmake|testlib|dbus|android|gradle|wasm|wayland|xcb|cocoa|forkfd|"
                  r"libpsl|sqlite|pixman|gradientgen|network)(/|$)")

shutil.rmtree(out, ignore_errors=True)
os.makedirs(f"{out}/licenses")

entries = []
for m in modules:
    for root, dirs, files in os.walk(f"{src}/{m}"):
        dirs.sort()
        rel = root[len(src):]
        if "qt_attribution.json" not in files or skip.search(rel):
            continue
        data = json.loads(open(f"{root}/qt_attribution.json", encoding="utf-8").read(), strict=False)
        for e in data if isinstance(data, list) else [data]:
            e["_dir"] = root
            entries.append(e)

seen_ids, uniq = set(), []
for e in entries:
    if e["Id"] in seen_ids or e["Id"] in {"qeventdispatcher_cf"}:  # macOS only
        continue
    seen_ids.add(e["Id"])
    uniq.append(e)
uniq.sort(key=lambda e: e["Name"].lower())

by_hash = {}
def license_file(e, path):
    full = os.path.normpath(os.path.join(e["_dir"], path))
    data = open(full, "rb").read()
    h = hashlib.sha256(data.replace(b"\r\n", b"\n").strip()).hexdigest()
    if h not in by_hash:
        base = os.path.basename(full)
        if not base.lower().endswith(".txt"):
            base += ".txt"
        name = f"{e['Id']}-{base}"
        name = re.sub(r"[^A-Za-z0-9._+-]", "_", name)
        while os.path.exists(f"{out}/licenses/{name}"):
            name = f"{e['Id']}-{name}"
        open(f"{out}/licenses/{name}", "wb").write(data.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))
        by_hash[h] = name
    return by_hash[h]

# Qt's own licence
for f in ["LGPL-3.0-only.txt", "GPL-3.0-only.txt"]:
    data = open(f"{src}/{modules[0]}/LICENSES/{f}", "rb").read()
    open(f"{out}/licenses/{f}", "wb").write(data.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))

L = []
w = L.append
w("ZenViewer for Windows - third-party notices")
w("=" * 44)
w("")
w("ZenViewer itself is under the MIT License (LICENSE.txt). This package also")
w("contains software by others, listed below with the licences it comes under.")
w("")
w("Qt")
w("--")
w(f"ZenViewer uses the Qt Toolkit, version {QT} (Qt6Core.dll, Qt6Gui.dll,")
w("Qt6Widgets.dll, Qt6Svg.dll and the plugins in the platforms, styles,")
w("imageformats and iconengines folders, all in the dll folder).")
w("Copyright (C) The Qt Company Ltd. and other contributors.")
w("")
w("Qt is used under the GNU Lesser General Public License version 3")
w("(licenses/LGPL-3.0-only.txt), which builds on the GNU General Public License")
w("version 3 (licenses/GPL-3.0-only.txt). Qt was not modified. You may replace")
w("the Qt DLLs and plugins in the dll folder with your own build of Qt "+QT.rsplit('.',1)[0]+".")
w("")
w("The complete source code of the Qt version used is available from")
w(f"https://download.qt.io/official_releases/qt/{QT.rsplit('.',1)[0]}/{QT}/submodules/ :")
for m in modules:
    w(f"  {m}.tar.xz")
w("and from the same place you obtained this package.")
w("")
w("Libraries and code included in Qt")
w("---------------------------------")
w("")
for e in uniq:
    title = e["Name"] + (f" {e['Version']}" if e.get("Version") else "")
    w(title)
    if e.get("QtUsage"):
        w("  " + e["QtUsage"].strip())
    cr = e.get("Copyright", [])
    for c in ([cr] if isinstance(cr, str) else cr):
        for line in c.splitlines():
            if line.strip():
                w("  " + line.strip())
    w("  Licence: " + e.get("License", e.get("LicenseId", "")))
    files = e.get("LicenseFile") or e.get("LicenseFiles") or []
    files = [files] if isinstance(files, str) else files
    names = [license_file(e, p) for p in files if p]
    if not names:  # no file of its own: use Qt's copy of each standard licence named
        std = f"{src}/{modules[0]}/LICENSES"
        for lid in re.findall(r"[A-Za-z0-9.+-]+", e.get("LicenseId", "")):
            if lid not in ("AND", "OR", "WITH") and os.path.exists(f"{std}/{lid}.txt"):
                names.append(license_file(e, os.path.relpath(f"{std}/{lid}.txt", e["_dir"])))
    if names:
        w("  Licence text: " + ", ".join(f"licenses/{n}" for n in names))
    w("")
# Required wording of the IJG licence
w("This software is based in part on the work of the Independent JPEG Group.")
w("Portions of this software are copyright (C) The FreeType Project")
w("(www.freetype.org). All rights reserved.")
w("")
open(f"{out}/THIRD-PARTY-NOTICES.txt", "w", encoding="utf-8", newline="\r\n").write("\n".join(L))
print(len(uniq), "components,", len(os.listdir(f"{out}/licenses")), "licence files")
