#!/usr/bin/env python3
"""Usage: notices.py <folder with the unpacked Qt sources> <list of bundled system libraries> <output folder>
Builds THIRD-PARTY-NOTICES.txt and licenses/ for the Linux bundle: Qt and the code
inside it from the qt_attribution.json files of the Qt sources, and the system
libraries copied into lib/ (one path per line in the list) from the copyright
files of their Ubuntu packages. container.sh runs it; the Windows package has its
own in windows/notices."""
import hashlib, json, os, re, subprocess, sys

src, libs_file, out = sys.argv[1], sys.argv[2], sys.argv[3]
QT = os.environ.get("QT_VERSION", "6.12.0")
modules = [f"qtbase-everywhere-src-{QT}", f"qtsvg-everywhere-src-{QT}", f"qtimageformats-everywhere-src-{QT}"]
# Code that is not in the libraries and plugins the bundle ships: other platforms,
# parts of Qt that are turned off, and FreeType, which comes from the system
skip = re.compile(r"/(tests|examples|doc|tools|cmake|testlib|testinternal|dbus|android|gradle|wasm|cocoa|wintab|"
                  r"D3D12MemoryAllocator|freetype|libpsl|sqlite|pixman|gradientgen|network)(/|$)")
skip_ids = {"qeventdispatcher_cf", "rhi-miniengine-d3d12-mipmap"}  # macOS, Windows

os.makedirs(f"{out}/licenses", exist_ok=True)

by_hash = {}
def license_file(prefix, full):
    """Copies a licence text into licenses/ once, however many components share it"""
    data = open(full, "rb").read().replace(b"\r\n", b"\n")
    h = hashlib.sha256(data.strip()).hexdigest()
    if h not in by_hash:
        base = os.path.basename(full)
        if not base.lower().endswith(".txt"):
            base += ".txt"
        name = re.sub(r"[^A-Za-z0-9._+-]", "_", f"{prefix}-{base}")
        while os.path.exists(f"{out}/licenses/{name}"):
            name = f"{prefix}-{name}"
        open(f"{out}/licenses/{name}", "wb").write(data)
        by_hash[h] = name
    return by_hash[h]

# Qt's own licence
std = f"{src}/{modules[0]}/LICENSES"
for f in ["LGPL-3.0-only.txt", "GPL-3.0-only.txt"]:
    open(f"{out}/licenses/{f}", "wb").write(open(f"{std}/{f}", "rb").read())

entries = []
for m in modules:
    for root, dirs, files in os.walk(f"{src}/{m}"):
        dirs.sort()
        if "qt_attribution.json" not in files or skip.search(root[len(src):]):
            continue
        data = json.loads(open(f"{root}/qt_attribution.json", encoding="utf-8").read(), strict=False)
        for e in data if isinstance(data, list) else [data]:
            e["_dir"] = root
            entries.append(e)
seen, uniq = set(), []
for e in entries:
    if e["Id"] not in seen and e["Id"] not in skip_ids:
        seen.add(e["Id"])
        uniq.append(e)
uniq.sort(key=lambda e: e["Name"].lower())

# The system libraries in lib/, grouped by the Ubuntu source package they come from
packages = {}
for path in open(libs_file).read().split():
    real = os.path.realpath(path)
    owner = subprocess.run(["dpkg", "-S", real], capture_output=True, text=True).stdout.split(":")[0]
    if not owner:
        sys.exit(f"FAILED: no Ubuntu package owns {path}, so its licence is unknown")
    source, version = subprocess.run(["dpkg-query", "-W", "-f", "${source:Package} ${Version}", owner],
                                     capture_output=True, text=True, check=True).stdout.split()
    copyright = f"/usr/share/doc/{owner}/copyright"
    if not os.path.exists(copyright):
        sys.exit(f"FAILED: {copyright} is missing, so the licence of {path} is unknown")
    package = packages.setdefault(source, {"version": version, "files": [], "copyright": copyright})
    package["files"].append(os.path.basename(path))

L = []
w = L.append
w("ZenViewer for Linux - third-party notices")
w("=" * 42)
w("")
w("ZenViewer itself is under the MIT License (LICENSE.txt). This package also")
w("contains software by others, listed below with the licences it comes under.")
w("")
w("Qt")
w("--")
w(f"ZenViewer uses the Qt Toolkit, version {QT} (the libQt6 libraries in the lib")
w("folder, and the plugins in its platforms, imageformats, iconengines,")
w("platforminputcontexts and wayland folders).")
w("Copyright (C) The Qt Company Ltd. and other contributors.")
w("")
w("Qt is used under the GNU Lesser General Public License version 3")
w("(licenses/LGPL-3.0-only.txt), which builds on the GNU General Public License")
w("version 3 (licenses/GPL-3.0-only.txt). Qt was not modified. You may replace")
w("the Qt libraries and plugins in the lib folder with your own build of Qt " + QT.rsplit(".", 1)[0] + ".")
w("")
w("The complete source code of the Qt version used is available from")
w(f"https://download.qt.io/official_releases/qt/{QT.rsplit('.', 1)[0]}/{QT}/submodules/ :")
for m in modules:
    w(f"  {m}.tar.xz")
w("and from the same place you obtained this package.")
w("")
# TinyEXIF is in the repository (third_party/TinyEXIF), not in Qt; container.sh copies its licences
w("TinyEXIF")
w("--------")
w("ZenViewer reads EXIF details with TinyEXIF 1.2.1, built into the zenviewer")
w("program. Copyright (c) 2015-2025 Seacave (cdcseacave); parts come from")
w("easyexif, Copyright (c) 2010-2015 Mayank Lahiri.")
w("Licence: MIT (licenses/TinyEXIF-LICENSE.txt); the parts from easyexif are also")
w("under the BSD-2-Clause licence (licenses/TinyEXIF-easyexif-LICENSE.txt).")
w("")
w("Libraries and code included in Qt")
w("---------------------------------")
w("")
for e in uniq:
    w(e["Name"] + (f" {e['Version']}" if e.get("Version") else ""))
    if e.get("QtUsage"):
        w("  " + " ".join(e["QtUsage"].split()))
    cr = e.get("Copyright", [])
    for c in ([cr] if isinstance(cr, str) else cr):
        for line in c.splitlines():
            if line.strip():
                w("  " + line.strip())
    w("  Licence: " + e.get("License", e.get("LicenseId", "")))
    files = e.get("LicenseFile") or e.get("LicenseFiles") or []
    files = [files] if isinstance(files, str) else files
    names = [license_file(e["Id"], os.path.normpath(os.path.join(e["_dir"], p))) for p in files if p]
    if not names:  # no file of its own: use Qt's copy of each standard licence named
        for lid in re.findall(r"[A-Za-z0-9.+-]+", e.get("LicenseId", "")):
            if lid not in ("AND", "OR", "WITH") and os.path.exists(f"{std}/{lid}.txt"):
                names.append(license_file(e["Id"], f"{std}/{lid}.txt"))
    if names:
        w("  Licence text: " + ", ".join(f"licenses/{n}" for n in names))
    w("")
# Required wording of the IJG licence, and of the FreeType one for Qt's rasterizer
w("This software is based in part on the work of the Independent JPEG Group.")
w("Portions of this software are copyright (C) The FreeType Project")
w("(www.freetype.org). All rights reserved.")
w("")
if packages:
    w("Other libraries in the lib folder")
    w("---------------------------------")
    w("These are unmodified copies from Ubuntu 22.04, named here by the Ubuntu source")
    w("package they belong to. Each licence text holds the copyright notices too.")
    w("")
    for source in sorted(packages):
        package = packages[source]
        w(f"{source} {package['version']}")
        files = sorted(package["files"])
        for i in range(0, len(files), 3):
            w("  " + ", ".join(files[i:i + 3]) + ("," if i + 3 < len(files) else ""))
        w("  Licence text: licenses/" + license_file(source, package["copyright"]))
        w("")
open(f"{out}/THIRD-PARTY-NOTICES.txt", "w", encoding="utf-8").write("\n".join(L))
print(len(uniq), "Qt components,", len(packages), "other packages,", len(os.listdir(f"{out}/licenses")), "licence files")
