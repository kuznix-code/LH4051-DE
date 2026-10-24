#!/bin/sh
# LH4051-DE.app entry point: macOS has no window manager hooks for a full shell,
# so the desktop runs inside a window (--windowed).
here="$(cd "$(dirname "$0")" && pwd)"
for p in /opt/homebrew /usr/local /opt/local; do
    [ -d "$p/share" ] && XDG_DATA_DIRS="${XDG_DATA_DIRS:+$XDG_DATA_DIRS:}$p/share"
    [ -d "$p/bin" ] && PATH="$PATH:$p/bin"
done
export XDG_DATA_DIRS PATH
exec "$here/lh4051-de-bin" --windowed "$@"
