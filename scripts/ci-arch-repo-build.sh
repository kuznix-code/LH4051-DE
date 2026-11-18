#!/usr/bin/env bash
# Build the complete LH4051-DE Arch split-package set inside an Arch/CachyOS container.
set -Eeuo pipefail

: "${BUILD_PROFILE:?BUILD_PROFILE must be arch or cachy}"
: "${PKGVER:?PKGVER is required}"
: "${SOURCE_COMMIT:?SOURCE_COMMIT is required}"
BUILD_MARCH="${BUILD_MARCH:-}"

if [[ "$(uname -m)" != "x86_64" ]]; then
    echo "error: these repositories currently build x86_64 packages only" >&2
    exit 2
fi

pacman -Syu --noconfirm --needed \
    base-devel git pkgconf gtk4 glib2 gtk4-layer-shell

MAKEPKG_CONFIG=/etc/makepkg.conf
if [[ "$BUILD_PROFILE" == "cachy" ]]; then
    : "${BUILD_MARCH:?BUILD_MARCH is required for CachyOS profiles}"
    MAKEPKG_CONFIG=/tmp/kuznix-makepkg.conf
    cat > "$MAKEPKG_CONFIG" <<EOF
source /etc/makepkg.conf
CPPFLAGS=""
CFLAGS="-march=${BUILD_MARCH} -O3 -pipe -fno-plt -fexceptions \\
        -Wp,-D_FORTIFY_SOURCE=3 -Wformat -Werror=format-security \\
        -fstack-clash-protection -fcf-protection"
CXXFLAGS="\$CFLAGS -Wp,-D_GLIBCXX_ASSERTIONS"
LDFLAGS="-Wl,-O1 -Wl,--sort-common -Wl,--as-needed -Wl,-z,relro -Wl,-z,now \\
         -Wl,-z,pack-relative-relocs"
LTOFLAGS="-flto=auto"
MAKEFLAGS="-j$(nproc)"
NINJAFLAGS="-j$(nproc)"
DEBUG_CFLAGS="-g"
DEBUG_CXXFLAGS="\$DEBUG_CFLAGS"
EOF
fi

build_dir=/build/lh4051-de
id builder >/dev/null 2>&1 || useradd --create-home builder
install -d -m 0755 -o builder -g builder "$build_dir" /out

sed \
    -e "s|@PKGVER@|${PKGVER}|g" \
    -e "s|@ARCH@|x86_64|g" \
    -e "s|@COMMIT@|${SOURCE_COMMIT}|g" \
    -e "s|@PKG_VERSION@|${PKGVER}|g" \
    /src/PKGBUILD-git.in > "$build_dir/PKGBUILD"
chown builder:builder "$build_dir/PKGBUILD"

echo "Building LH4051-DE ${PKGVER} from ${SOURCE_COMMIT}"
echo "Profile: ${BUILD_PROFILE}${BUILD_MARCH:+ (${BUILD_MARCH})}"
su -s /bin/bash builder -c \
    "cd '$build_dir' && MAKEPKG_CONF='$MAKEPKG_CONFIG' PKGDEST=/out makepkg --noconfirm --cleanbuild"

package_count="$(find /out -maxdepth 1 -type f -name '*.pkg.tar.zst' | wc -l)"
if [[ "$package_count" -lt 15 ]]; then
    echo "error: expected all 15 split packages, got ${package_count}" >&2
    find /out -maxdepth 1 -type f -printf '%f\n' >&2
    exit 1
fi
ls -lh /out
