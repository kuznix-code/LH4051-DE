#!/bin/sh
# check-win7.sh <dir-or-exe>...
# Reports imported Windows APIs that do not exist on Windows 7 / 8 / 8.1 in an exe or in every
# .exe/.dll under a directory (e.g. a bundled GTK runtime). Exit status 1 if any were found.
# Uses objdump (binutils or llvm-objdump); the list is a deliberate sample of common newer APIs.
set -eu
OBJDUMP="${OBJDUMP:-objdump}"
command -v "$OBJDUMP" >/dev/null 2>&1 || OBJDUMP=llvm-objdump
command -v "$OBJDUMP" >/dev/null 2>&1 || { echo "check-win7: objdump not found" >&2; exit 2; }

# api:first Windows version that has it
NEWER='GetSystemTimePreciseAsFileTime:8
SetThreadDescription:10
GetThreadDescription:10
GetDpiForWindow:10
GetDpiForSystem:10
GetDpiForMonitor:8.1
SetProcessDpiAwarenessContext:10
AreDpiAwarenessContextsEqual:10
EnableNonClientDpiScaling:10
GetSystemMetricsForDpi:10
AdjustWindowRectExForDpi:10
SetProcessDpiAwareness:8.1
GetCurrentPackageFullName:8
CreateFile2:8
PrefetchVirtualMemory:8
WaitOnAddress:8
WakeByAddressSingle:8
WakeByAddressAll:8
GetTempPath2W:11
SetWindowCompositionAttribute:10
RtlGetDeviceFamilyInfoEnum:10'

status=0
files=$(for a in "$@"; do
    if [ -d "$a" ]; then find "$a" -type f \( -iname '*.exe' -o -iname '*.dll' \); else echo "$a"; fi
done)
[ -n "$files" ] || { echo "check-win7: nothing to scan" >&2; exit 2; }

for f in $files; do
    [ -f "$f" ] || { echo "check-win7: $f not found" >&2; exit 2; }
    dump=$("$OBJDUMP" -p "$f" 2>/dev/null) || { echo "check-win7: cannot read $f" >&2; exit 2; }
    imports=$(printf '%s\n' "$dump" | sed -n 's/^.*[[:space:]]\([A-Za-z0-9_]\{4,\}\)$/\1/p' | sort -u)
    for entry in $NEWER; do
        api="${entry%%:*}"; since="${entry##*:}"
        if printf '%s\n' "$imports" | grep -qx "$api"; then
            echo "$f imports $api (needs Windows $since or newer)"
            status=1
        fi
    done
done
[ $status = 0 ] && echo "check-win7: no imports newer than Windows 7 found in $(echo "$files" | wc -l) file(s)"
exit $status
