#!/bin/sh
# Build, smoke-test and package LH4051-DE inside a distribution container (used by CI).
#
#   PM=apt|apk        package manager of the image
#   DIST_ARCH=<name>  architecture name used in the dist/installer file names (e.g. musl-i686, riscv64)
#   PKG_VERSION=...   version for the file names (from scripts/pkg-version.sh on the host)
#   ALLOW_SKIP=1      exit successfully, without packaging, when the distribution has no GTK 4.10+
#   MARCH=...         optional -march for the build
#
# Run from the repository root.
set -eu
: "${PM:?PM must be apt or apk}"
: "${DIST_ARCH:?DIST_ARCH is required}"

case "$PM" in
    apk)
        apk add --no-cache build-base pkgconf make gtk4.0-dev glib-dev tar
        apk add --no-cache gtk4-layer-shell-dev || echo "gtk4-layer-shell not available; building composite-only"
        ;;
    apt)
        export DEBIAN_FRONTEND=noninteractive
        apt-get update
        apt-get install -y --no-install-recommends build-essential pkg-config make tar ca-certificates \
            libgtk-4-dev libglib2.0-dev-bin
        apt-get install -y --no-install-recommends libgtk4-layer-shell-dev || echo "gtk4-layer-shell not available; building composite-only"
        ;;
    *) echo "unknown PM '$PM'" >&2; exit 2 ;;
esac

echo "machine: $(uname -m)   gcc: $(gcc --version | head -n 1)   GTK: $(pkg-config --modversion gtk4 2>/dev/null || echo none)"
if ! pkg-config --atleast-version=4.10 gtk4 2>/dev/null; then
    if [ "${ALLOW_SKIP:-0}" = 1 ]; then
        echo "::notice::Skipped $DIST_ARCH: this distribution only has GTK $(pkg-config --modversion gtk4 2>/dev/null || echo none); LH4051-DE needs GTK 4.10 or newer."
        exit 0
    fi
    echo "error: GTK 4.10 or newer is required" >&2
    exit 1
fi

JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
make TARGET_OS=linux ${MARCH:+MARCH="$MARCH"} show-config
make TARGET_OS=linux ${MARCH:+MARCH="$MARCH"} -j"$JOBS" all
./build/linux-gcc/lh4051-de --version
make TARGET_OS=linux ${MARCH:+MARCH="$MARCH"} DIST_ARCH="$DIST_ARCH" dist-linux installer-unix
ls -l dist
