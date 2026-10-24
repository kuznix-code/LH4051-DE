#!/bin/sh
# LH4051-DE Installer (self-extracting)  --  version @VERSION@
# Usage: sh LH4051-DE-Installer-*.run [--prefix DIR] [--yes] [--uninstall] [--help]
set -eu
PREFIX=/usr/local
YES=0
UNINSTALL=0
while [ $# -gt 0 ]; do
    case "$1" in
        --prefix) PREFIX="$2"; shift ;;
        --prefix=*) PREFIX="${1#--prefix=}" ;;
        -y|--yes) YES=1 ;;
        --uninstall) UNINSTALL=1 ;;
        -h|--help)
            echo "LH4051-DE Installer @VERSION@"
            echo "  --prefix DIR   install under DIR (default /usr/local)"
            echo "  --yes          do not ask for confirmation"
            echo "  --uninstall    remove a previous installation from the prefix"
            exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

SUDO=""
if [ "$(id -u)" != 0 ]; then
    mkdir -p "$PREFIX" 2>/dev/null || true
    if [ ! -w "$PREFIX" ]; then
        for c in sudo doas; do command -v $c >/dev/null 2>&1 && { SUDO=$c; break; }; done
        [ -n "$SUDO" ] || { echo "error: $PREFIX is not writable and neither sudo nor doas exists; use --prefix or run as root" >&2; exit 1; }
    fi
fi

MANIFEST="$PREFIX/share/lh4051-de/install-manifest.txt"
if [ "$UNINSTALL" = 1 ]; then
    [ -f "$MANIFEST" ] || { echo "No installation found under $PREFIX"; exit 1; }
    while IFS= read -r f; do $SUDO rm -f "$f"; done < "$MANIFEST"
    $SUDO rm -f "$MANIFEST"; $SUDO rmdir "$PREFIX/share/lh4051-de" 2>/dev/null || true
    echo "LH4051-DE removed from $PREFIX"
    exit 0
fi

echo "LH4051-DE Installer @VERSION@"
echo "  install prefix: $PREFIX"
if [ "$YES" != 1 ] && [ -t 0 ]; then
    printf 'Continue? [Y/n] '
    read -r a
    case "$a" in n|N|no|NO) echo "Cancelled."; exit 1 ;; esac
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
LINE="$(awk '/^__LH4051_PAYLOAD_BELOW__$/ { print NR + 1; exit }' "$0")"
tail -n +"$LINE" "$0" | tar -xzf - -C "$TMP"

# The payload was staged with PREFIX=/usr; relocate to the chosen prefix.
$SUDO mkdir -p "$PREFIX"
( cd "$TMP/usr" && find . -type f | sed 's|^\./||' ) > "$TMP/files.txt"
while IFS= read -r f; do
    $SUDO mkdir -p "$PREFIX/$(dirname "$f")"
    $SUDO cp "$TMP/usr/$f" "$PREFIX/$f"
done < "$TMP/files.txt"
# session/desktop files embed /usr/bin; point them at the real prefix
if [ "$PREFIX" != /usr ]; then
    for f in share/applications/lh4051-de.desktop share/wayland-sessions/lh4051-de.desktop share/xsessions/lh4051-de.desktop; do
        [ -f "$PREFIX/$f" ] && $SUDO sed -i.bak "s|/usr/bin|$PREFIX/bin|g" "$PREFIX/$f" && $SUDO rm -f "$PREFIX/$f.bak"
    done
fi
sed "s|^|$PREFIX/|" "$TMP/files.txt" > "$TMP/manifest"
$SUDO mkdir -p "$PREFIX/share/lh4051-de"
$SUDO cp "$TMP/manifest" "$MANIFEST"

echo "Installed. Run 'lh4051-de --windowed' or pick the LH4051-DE session at login."
echo "Uninstall with: sh $(basename "$0") --uninstall --prefix $PREFIX"
exit 0
__LH4051_PAYLOAD_BELOW__
