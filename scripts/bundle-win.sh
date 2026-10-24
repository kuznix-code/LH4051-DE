#!/bin/sh
# Bundle lh4051-de.exe with the GTK4 runtime it needs (MinGW-w64 build).
#   bundle-win.sh <exe> <outdir> [triple]
# Works from an MSYS2 MINGW64 shell (uses ldd) or when cross-compiling from Linux
# (uses objdump to walk DLL dependencies in the MinGW sysroot).
set -eu
exe="$1"; out="$2"; triple="${3:-x86_64-w64-mingw32}"
[ -f "$exe" ] || { echo "bundle-win: $exe not found (run 'make win64' first)" >&2; exit 1; }

# Locate the MinGW prefix that holds bin/, lib/, share/.
if [ -n "${MINGW_PREFIX:-}" ] && [ -d "$MINGW_PREFIX/bin" ]; then
    prefix="$MINGW_PREFIX"
elif [ -d "/usr/$triple" ]; then
    prefix="/usr/$triple"
else
    echo "bundle-win: set MINGW_PREFIX to the MinGW sysroot (e.g. /mingw64)" >&2; exit 1
fi
bin="$prefix/bin"
[ -d "$bin" ] || bin="$prefix/sys-root/mingw/bin"

rm -rf "$out"; mkdir -p "$out"
cp "$exe" "$out/"

objdump_bin="objdump"
if command -v "$triple-objdump" >/dev/null 2>&1; then objdump_bin="$triple-objdump"
elif ! command -v objdump >/dev/null 2>&1 && command -v llvm-objdump >/dev/null 2>&1; then objdump_bin="llvm-objdump"; fi

# Resolve DLL dependency closure by name against $bin.
todo="$out/$(basename "$exe")"
seen=""
while [ -n "$todo" ]; do
    cur="${todo%% *}"
    case "$todo" in *" "*) todo="${todo#* }";; *) todo="";; esac
    for dll in $("$objdump_bin" -p "$cur" 2>/dev/null | sed -n 's/^[[:space:]]*DLL Name: //p'); do
        case " $seen " in *" $dll "*) continue;; esac
        lc=$(printf '%s' "$dll" | tr 'A-Z' 'a-z')
        src=""
        for cand in "$bin/$dll" "$bin/$lc"; do [ -f "$cand" ] && src="$cand" && break; done
        [ -z "$src" ] && continue            # system DLL (kernel32, user32, ...)
        seen="$seen $dll"
        cp "$src" "$out/"
        todo="$todo $out/$dll"
    done
done

# GTK runtime data: schemas, icon themes, image loaders, settings.
for d in share/glib-2.0/schemas share/icons/Adwaita share/icons/hicolor lib/gdk-pixbuf-2.0 \
         lib/gio libexec/glycin-loaders share/glycin-loaders lib/glycin-loaders share/gtk-4.0; do
    if [ -d "$prefix/$d" ]; then mkdir -p "$out/$(dirname "$d")"; cp -r "$prefix/$d" "$out/$(dirname "$d")/"; fi
done
if command -v glib-compile-schemas >/dev/null 2>&1 && [ -d "$out/share/glib-2.0/schemas" ]; then
    glib-compile-schemas "$out/share/glib-2.0/schemas"
fi

# Launcher that keeps GTK on the bundled runtime and offers a console for debugging.
cat > "$out/lh4051-de-debug.bat" <<'BAT'
@echo off
rem Runs LH4051-DE in a window so it can be closed easily, and keeps this console open.
set GSK_RENDERER=
"%~dp0lh4051-de.exe" --windowed %*
pause
BAT

if command -v zip >/dev/null 2>&1; then
    (cd "$(dirname "$out")" && rm -f "$(basename "$out").zip" && zip -qr "$(basename "$out").zip" "$(basename "$out")")
    echo "==> $out.zip"
fi
echo "==> $out"
