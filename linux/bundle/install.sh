#!/bin/sh
# Adds ZenViewer to the app menu and the PATH, run from this folder where it is now.
# As a user it installs for you alone: ~/.local/share and ~/.local/bin (no root needed).
# With sudo it installs for everyone: /usr/local/share and /usr/local/bin.
# Run it again after moving the folder; "./install.sh --uninstall" removes it again
# (with sudo, if it was installed with sudo).
set -e
dir=$(cd "$(dirname "$0")" && pwd)
if [ "$(id -u)" = 0 ]; then
    data=/usr/local/share
    bin=/usr/local/bin
    who="everyone"
else
    data=${XDG_DATA_HOME:-$HOME/.local/share}
    bin=$HOME/.local/bin
    who="you"
fi
apps=$data/applications
icons=$data/icons/hicolor
link=$bin/zenviewer

refresh() {
    command -v update-desktop-database >/dev/null && update-desktop-database -q "$apps" 2>/dev/null || true
    command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q -t "$icons" 2>/dev/null || true
}

if [ "$1" = "--uninstall" ]; then
    rm -f "$apps/zenviewer.desktop"
    (cd "$dir/icons/hicolor" && find . -name 'zenviewer.*') | while read -r icon; do
        rm -f "$icons/$icon"
    done
    # Only a link this script made; a zenviewer installed some other way stays
    [ -L "$link" ] && [ "$(readlink "$link")" = "$dir/zenviewer" ] && rm -f "$link"
    refresh
    echo "Removed ZenViewer from the app menu and $bin for $who. Delete this folder to remove the app itself."
    exit 0
fi

# Exec takes a quoted path: ", `, $ and \ get a backslash, and the file format
# doubles every backslash. TryExec is a plain path, so it only doubles them.
exec_path=$(printf '%s' "$dir/zenviewer" | sed -e 's/\\/\\\\\\\\/g' -e 's/["`$]/\\\\&/g')
try_path=$(printf '%s' "$dir/zenviewer" | sed -e 's/\\/\\\\/g')
mkdir -p "$apps" "$icons" "$bin"
cp -r "$dir/icons/hicolor/." "$icons/"
while IFS= read -r line; do
    case "$line" in
        Exec=*) printf 'Exec="%s" %%f\nTryExec=%s\n' "$exec_path" "$try_path" ;;
        *) printf '%s\n' "$line" ;;
    esac
done <"$dir/zenviewer.desktop" >"$apps/zenviewer.desktop"
refresh
echo "Added ZenViewer to the app menu for $who, running $dir/zenviewer"

# The command: a link to the binary, which still finds lib/ beside the real file
if [ -e "$link" ] && [ ! -L "$link" ]; then
    echo "Left $link alone: it is another zenviewer, not a link to this folder."
else
    ln -sfn "$dir/zenviewer" "$link"
    echo "Added the zenviewer command, in $bin"
    case ":$PATH:" in
        *":$bin:"*) ;;
        *) echo "$bin isn't in your PATH yet; add it in your shell's profile, or log out and in again." ;;
    esac
fi
