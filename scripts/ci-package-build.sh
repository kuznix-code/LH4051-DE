#!/bin/sh
# Builds distribution packages (from source) inside a distribution container. Used by CI
# (.github/workflows/packages.yml). Run from the repository root.
#
#   FORMAT=deb|rpm|arch   package format (the container must be Debian, Fedora or Arch based)
#   PKG_VERSION=...       project version (scripts/pkg-version.sh), e.g. 0.2.0.dev.20261101.a31e706 or 0.1.1
#   BUILD_DATA=1|0        keep the architecture-independent lh4051-de-data package (1) or only the
#                         architecture-specific lh4051-de package (0). CI sets 1 on x86_64 only.
#
# Output: dist/*.deb, dist/*.rpm or dist/*.pkg.tar.zst
set -eu
: "${FORMAT:?FORMAT must be deb, rpm or arch}"
: "${PKG_VERSION:?PKG_VERSION is required}"
BUILD_DATA="${BUILD_DATA:-1}"

SRC="$(pwd)"
WORK="${TMPDIR:-/tmp}/lh4051-de-pkg"
OUT="$SRC/dist"
rm -rf "$WORK"; mkdir -p "$WORK" "$OUT"

# Per-format version spellings of the same upstream version.
#   deb/rpm: pre-releases use "~" so they sort before the final release (0.2.0~dev... < 0.2.0)
#   arch:    no "~" or "-" allowed in pkgver
VER_TILDE="$(printf '%s' "$PKG_VERSION" | sed -E 's/-/~/; s/\.(dev|alpha|beta|rc)/~\1/')"
VER_ARCH="$(printf '%s' "$PKG_VERSION" | tr '~-' '__')"
echo "package version: $PKG_VERSION  (deb/rpm: $VER_TILDE, arch: $VER_ARCH)   machine: $(uname -m)"

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

    dir="$WORK/lh4051-de-$VER_TILDE"
    copy_tree "$dir"
    cat > "$dir/debian/changelog" <<CHANGELOG
lh4051-de ($VER_TILDE) unstable; urgency=medium

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
    copy_tree "$WORK/lh4051-de-$VER_TILDE"
    tar -C "$WORK" -czf "$top/SOURCES/lh4051-de-$VER_TILDE.tar.gz" "lh4051-de-$VER_TILDE"
    sed -e "s|@VERSION@|$VER_TILDE|g" -e "s|@PKG_VERSION@|$PKG_VERSION|g" -e "s|@DATE@|$(date -u '+%a %b %d %Y')|g" \
        packaging/rpm/lh4051-de.spec.in > "$top/SPECS/lh4051-de.spec"
    rpmbuild -bb "$top/SPECS/lh4051-de.spec"
    for f in "$top"/RPMS/*/*.rpm; do
        if [ "$BUILD_DATA" != 1 ] && [ "${f%.noarch.rpm}" != "$f" ]; then continue; fi
        cp "$f" "$OUT"/
    done
    ;;

arch)
    # pacman's download sandbox (Landlock) does not work inside containers; CheckSpace cannot see the mount.
    PM="pacman --noconfirm --needed"
    # (--disable-sandbox is not listed in --help on every pacman 7.x, so probe it instead of grepping.)
    if pacman --disable-sandbox --version >/dev/null 2>&1; then PM="$PM --disable-sandbox"; fi
    sed -i 's/^CheckSpace/#CheckSpace/' /etc/pacman.conf
    # Arch Linux ARM's root filesystem ships without an initialised keyring.
    if [ ! -s /etc/pacman.d/gnupg/pubring.kbx ] && [ ! -s /etc/pacman.d/gnupg/pubring.gpg ]; then
        pacman-key --init
        for k in /usr/share/pacman/keyrings/*.gpg; do pacman-key --populate "$(basename "$k" .gpg)"; done
    fi
    $PM -Syu base-devel pkgconf gtk4 glib2 gtk4-layer-shell tar

    # makepkg refuses to run as root.
    id builder >/dev/null 2>&1 || useradd -m builder
    bdir="/home/builder/build"
    rm -rf "$bdir"; mkdir -p "$bdir"
    copy_tree "$WORK/lh4051-de-$VER_ARCH"
    tar -C "$WORK" -czf "$bdir/lh4051-de-$VER_ARCH.tar.gz" "lh4051-de-$VER_ARCH"
    arch="$(uname -m)"
    sed -e "s|@PKGVER@|$VER_ARCH|g" -e "s|@PKG_VERSION@|$PKG_VERSION|g" -e "s|@ARCH@|$arch|g" \
        packaging/arch/PKGBUILD.in > "$bdir/PKGBUILD"
    chown -R builder: "$bdir"
    su builder -c "cd '$bdir' && PKGDEST='$bdir/out' PKGEXT='.pkg.tar.zst' makepkg -f --nocolor --noconfirm"
    ls -l "$bdir/out"
    for f in "$bdir"/out/*.pkg.tar.zst; do
        if [ "$BUILD_DATA" != 1 ] && [ "${f%-any.pkg.tar.zst}" != "$f" ]; then continue; fi
        cp "$f" "$OUT"/
    done
    ;;
*)
    echo "unknown FORMAT '$FORMAT'" >&2; exit 2 ;;
esac

ls -l "$OUT"
