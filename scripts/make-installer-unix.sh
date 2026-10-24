#!/bin/sh
# make-installer-unix.sh <payload.tar.gz> <output.run> <version>
# Builds the self-extracting "LH4051-DE Installer" for unix-like systems.
set -eu
payload="$1"; out="$2"; version="$3"
here="$(cd "$(dirname "$0")" && pwd)"
sed "s/@VERSION@/$version/g" "$here/../installer/unix-header.sh" > "$out"
cat "$payload" >> "$out"
chmod 755 "$out"
echo "==> $out"
