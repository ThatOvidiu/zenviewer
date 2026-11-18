ZenViewer
=========

A lightweight image viewer. This folder is the whole app: it carries its own Qt
in the lib folder, so it needs no Qt on the system and can live anywhere.

Run it
------
  ./zenviewer                    open the viewer
  ./zenviewer path/to/images     open an image or a folder
  ./zenviewer -h                 list the keys and mouse actions (H in the app)

Add it to the app menu and the PATH
-----------------------------------
  ./install.sh                   menu entry, "Open with" for images and the
                                 zenviewer command, for you alone
  sudo ./install.sh              the same for everyone on this computer
  ./install.sh --uninstall       take them out again (with sudo if installed with sudo)

As a user, install.sh needs no root and only writes to ~/.local/share and
~/.local/bin. With sudo it writes to /usr/local/share and /usr/local/bin; keep
the folder somewhere everyone can read, such as /opt/zenviewer. The entry and
the command point at this folder, so run install.sh again after moving it.

What the system needs
---------------------
An x86-64 Linux desktop with glibc 2.35 or newer: Ubuntu 22.04+, Debian 12+,
Fedora 36+, RHEL 10, Arch and other current distributions.

On X11, ZenViewer also needs the libxkbcommon-x11 library, which a few desktops
don't install by themselves. If it doesn't start and prints

  qt.qpa.plugin: Could not load the Qt platform plugin "xcb" ...

(the line before it names xcb-cursor0, which is not the cause), install it:

  Debian, Ubuntu    sudo apt install libxkbcommon-x11-0
  Fedora, RHEL      sudo dnf install libxkbcommon-x11
  Arch              sudo pacman -S libxkbcommon-x11

Wayland sessions don't need it.

Licences
--------
ZenViewer is under the MIT License (LICENSE.txt). The licences of Qt and the
other software in this folder are in THIRD-PARTY-NOTICES.txt and licenses/.
