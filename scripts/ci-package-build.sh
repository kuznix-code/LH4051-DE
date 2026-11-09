#!/bin/sh
# Builds distribution packages (from source) inside a distribution container. Used by CI
# (.github/workflows/packages.yml). Run from the repository root.
#
#   FORMAT=deb|rpm|arch|apk  package format (the container must be Debian, Fedora, Arch or Alpine (musl) based)
#   PKG_VERSION=...       project version (scripts/pkg-version.sh), e.g. 0.2.0.dev.20261101.a31e706 or 0.1.1
#   BUILD_DATA=1|0        keep the architecture-independent packages (lh4051-de-data, lh4051-de-session,
#                         lh4051-de-session-wayland) (1) or only the architecture-specific lh4051-de
#                         package (0). CI sets 1 on x86_64 only.
#
# Output: dist/*.deb, dist/*.rpm, dist/*.pkg.tar.zst or dist/*.apk
set -eu
: "${FORMAT:?FORMAT must be deb, rpm, arch or apk}"
: "${PKG_VERSION:?PKG_VERSION is required}"
BUILD_DATA="${BUILD_DATA:-1}"

SRC="$(pwd)"
WORK="${TMPDIR:-/tmp}/lh4051-de-pkg"
OUT="$SRC/dist"
rm -rf "$WORK"; mkdir -p "$WORK" "$OUT"

# Per-format package versions are derived from the nearest reachable upstream tag.
DATE_UTC="$(date -u +%Y%m%d)"
TAG_REF="$(git describe --tags --abbrev=0 HEAD 2>/dev/null || true)"
if [ -n "$TAG_REF" ]; then
    TAG="${TAG_REF#v}"
    COMMITS="$(git rev-list --count "$TAG_REF"..HEAD)"
else
    TAG="$(sed -n 's/^#define LH4051_VERSION "\(.*\)"/\1/p' src/longhorn.h)"
    COMMITS="$(git rev-list --count HEAD)"
fi
SHORT_COMMIT="$(git rev-parse --short=7 HEAD)"
TODAY_COMMITS="$(git rev-list --count --since="$(date -u +%Y-%m-%dT00:00:00Z)" HEAD)"
PKGREL="${PKGREL:-1}"
VER_RPM="$TAG"
RPM_RELEASE="$PKGREL.$DATE_UTC"git"$SHORT_COMMIT"
VER_ARCH="$TAG.r$COMMITS.$SHORT_COMMIT"
VER_DEB="$TAG+git$DATE_UTC-$PKGREL+$TODAY_COMMITS"
VER_APK="$(printf '%s' "$PKG_VERSION" | sed -E 's/-(alpha|beta|rc)\.?([0-9]+)$/_\1\2/')"
case "$VER_APK" in
    *_alpha[0-9]*|*_beta[0-9]*|*_rc[0-9]*) ;;
    *)
        apk_base="$(printf '%s' "$VER_APK" | sed -nE 's/^([0-9]+(\.[0-9]+){0,2}).*/\1/p')"
        apk_day="$(printf '%s' "$VER_APK" | sed -nE 's/.*\.([0-9]{8})(\..*)?$/\1/p')"
        if [ "$VER_APK" != "$apk_base" ] && [ -n "$apk_day" ]; then VER_APK="${apk_base}_git${apk_day}"; else VER_APK="$apk_base"; fi
        ;;
esac
echo "package versions: deb=$VER_DEB rpm=$VER_RPM-$RPM_RELEASE arch=$VER_ARCH-$PKGREL apk=$VER_APK   machine: $(uname -m)"
# Copy of the working tree without VCS data and previous build output.
copy_tree() {
    mkdir -p "$1"
    tar -C "$SRC" --exclude=./.git --exclude=./build --exclude=./dist --exclude=./out -cf - . | tar -C "$1" -xf -
}

case "$FORMAT" in
deb)
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends build-essential debhelper dpkg-dev pkgconf ca-certificates tar \
        libgtk-4-dev libglib2.0-dev-bin
    apt-get install -y --no-install-recommends libgtk4-layer-shell-dev || echo "gtk4-layer-shell not available; building composite-only"

    dir="$WORK/lh4051-de-$VER_DEB"
    copy_tree "$dir"
    cat > "$dir/debian/changelog" <<CHANGELOG
lh4051-de ($VER_DEB) unstable; urgency=medium

  * Automated build of $PKG_VERSION.

 -- kuznix-code <noreply@github.com>  $(date -R)
CHANGELOG
    # -b: architecture-specific and -all packages; -B: architecture-specific only.
    if [ "$BUILD_DATA" = 1 ]; then kind=-b; else kind=-B; fi
    (cd "$dir" && PKG_VERSION="$PKG_VERSION" dpkg-buildpackage -us -uc -jauto "$kind")
    cp "$WORK"/*.deb "$OUT"/
    ;;

rpm)
    dnf -y install rpm-build gcc make tar gzip pkgconf-pkg-config gtk4-devel glib2-devel gtk4-layer-shell-devel
    top="$HOME/rpmbuild"
    mkdir -p "$top/SOURCES" "$top/SPECS"
    source_dir="lh4051-de-$VER_RPM"
    copy_tree "$WORK/$source_dir"
    tar -C "$WORK" -czf "$top/SOURCES/LH4051-DE-$VER_RPM.tar.gz" "$source_dir"
    sed -e "s|@VERSION@|$VER_RPM|g" -e "s|@RELEASE@|$RPM_RELEASE|g" \
        -e "s|@PKG_VERSION@|$PKG_VERSION|g" -e "s|@DATE@|$(date -u '+%a %b %d %Y')|g" \
        packaging/rpm/lh4051-de.spec.in > "$top/SPECS/LH4051-DE.spec"
    rpmbuild -bb "$top/SPECS/LH4051-DE.spec"
    for f in "$top"/RPMS/*/*.rpm; do
        if [ "$BUILD_DATA" != 1 ] && [ "${f%.noarch.rpm}" != "$f" ]; then continue; fi
        cp "$f" "$OUT"/
    done
    ;;
arch)
    PM="pacman --noconfirm --needed"
    if pacman --disable-sandbox --version >/dev/null 2>&1; then PM="$PM --disable-sandbox"; fi
    sed -i 's/^CheckSpace/#CheckSpace/' /etc/pacman.conf
    if [ ! -s /etc/pacman.d/gnupg/pubring.kbx ] && [ ! -s /etc/pacman.d/gnupg/pubring.gpg ]; then
        pacman-key --init
        for k in /usr/share/pacman/keyrings/*.gpg; do pacman-key --populate "$(basename "$k" .gpg)"; done
    fi
    $PM -Syu base-devel pkgconf gtk4 glib2 gtk4-layer-shell tar
    id builder >/dev/null 2>&1 || useradd -m builder
    bdir="/home/builder/build"
    rm -rf "$bdir"; mkdir -p "$bdir"
    cp -a "$SRC"/. "$bdir"/
    arch="$(uname -m)"
    sed -e "s|@PKGVER@|$VER_ARCH|g" -e "s|@ARCH@|$arch|g" \
        -e "s|@COMMIT@|$(git rev-parse HEAD)|g" -e "s|@PKG_VERSION@|$PKG_VERSION|g" PKGBUILD-git.in > "$bdir/PKGBUILD"
    chown -R builder: "$bdir"
    su builder -c "cd '$bdir' && PKGDEST='$bdir/out' PKGEXT='.pkg.tar.zst' makepkg -f --nocolor --noconfirm"
    for f in "$bdir"/out/*.pkg.tar.zst; do
        if [ "$BUILD_DATA" != 1 ] && [ "${f%-any.pkg.tar.zst}" != "$f" ]; then continue; fi
        cp "$f" "$OUT"/
    done
    ;;
apk)
    apk add --no-cache alpine-sdk tar pkgconf gtk4.0-dev glib-dev
    extra=""
    if apk add --no-cache gtk4-layer-shell-dev; then extra="gtk4-layer-shell-dev"; else echo "gtk4-layer-shell not available; building composite-only"; fi

    bdir="$WORK/apk"
    mkdir -p "$bdir"
    copy_tree "$WORK/lh4051-de-$VER_APK"
    tar -C "$WORK" -czf "$bdir/lh4051-de-$VER_APK.tar.gz" "lh4051-de-$VER_APK"
    sed -e "s|@PKGVER@|$VER_APK|g" -e "s|@PKG_VERSION@|$PKG_VERSION|g" -e "s|@MAKEDEPENDS_EXTRA@|$extra|g" \
        packaging/alpine/APKBUILD.in > "$bdir/APKBUILD"

    # abuild signs packages; a throwaway key is enough for CI artifacts. Building as root needs -F.
    abuild-keygen -a -i -n
    (cd "$bdir" && abuild -F checksum && REPODEST="$bdir/repo" abuild -F -r)
    find "$bdir/repo" -name '*.apk' -exec ls -l {} +
    for f in $(find "$bdir/repo" -name '*.apk'); do
        case "$(basename "$f")" in
            lh4051-de-data-*|lh4051-de-session-*) [ "$BUILD_DATA" = 1 ] || continue ;;
            *-doc-*|*-dbg-*|*-dev-*) continue ;;
        esac
        cp "$f" "$OUT"/
    done
    ;;
*)
    echo "unknown FORMAT '$FORMAT'" >&2; exit 2 ;;
esac

ls -l "$OUT"
