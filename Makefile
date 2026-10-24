# LH4051-DE — GNU make build system
#
#   Linux (gcc/clang)        make
#   macOS (Intel/ARM)        make                (Homebrew: brew install gtk4 pkgconf)
#   FreeBSD/OpenBSD/NetBSD   gmake               (GNU make is required; pkg install gmake gtk4)
#   Windows, MinGW-w64       make win64          (MSYS2 MINGW64 shell, or cross from Linux)
#   Windows, MSVC            make msvc GTK_PREFIX=C:/gtk   (run from a VS developer prompt)
#
# Run `make help` for all targets and variables.

NAME    := lh4051-de
VERSION := $(shell sed -n 's/^\#define LH4051_VERSION "\(.*\)"/\1/p' src/longhorn.h)

# ---------------------------------------------------------------------------
# Host / target selection
# ---------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
  HOST_OS := windows
else
  UNAME_S := $(shell uname -s 2>/dev/null)
  ifeq ($(UNAME_S),Linux)
    HOST_OS := linux
  else ifeq ($(UNAME_S),Darwin)
    HOST_OS := macos
  else ifneq ($(filter FreeBSD OpenBSD NetBSD DragonFly MidnightBSD,$(UNAME_S)),)
    HOST_OS := bsd
  else
    HOST_OS := unix
  endif
endif

TARGET_OS ?= $(HOST_OS)
# Any non-Windows target is unix-like (Linux, macOS, BSD, other).
UNIX_LIKE := $(filter linux macos bsd unix,$(TARGET_OS))
# TOOLCHAIN: gcc | clang | mingw | msvc
TOOLCHAIN ?= $(if $(filter windows,$(TARGET_OS)),mingw,gcc)
TRIPLE    ?= x86_64-w64-mingw32
IS_CROSS  := $(if $(and $(filter-out windows,$(HOST_OS)),$(filter windows,$(TARGET_OS))),1,)

# DEBUG=1: -O0 -g, no hardening.  LTO=1: link-time optimisation (gcc/clang).
# MARCH: e.g. x86-64-v3 or native.  CONSOLE=1: keep a console window on Windows.
# LAYER_SHELL: auto | 1 | 0  (Linux/BSD: gtk4-layer-shell support).
DEBUG   ?= 0
LTO     ?= 0
MARCH   ?=
CONSOLE ?= 0
LAYER_SHELL ?= auto
# WIN_ARCH only names the Windows bundle (x64, arm64).
WIN_ARCH ?= x64
# DIST_ARCH names unix tarballs/installers (default: uname -m).
DIST_ARCH ?= $(shell uname -m 2>/dev/null | sed 's/amd64/x86_64/;s/aarch64/arm64/')

PREFIX  ?= /usr/local
DESTDIR ?=

ifeq ($(TARGET_OS),windows)
  EXE := .exe
else
  EXE :=
endif
ifeq ($(TOOLCHAIN),msvc)
  OBJEXT := .obj
else
  OBJEXT := .o
endif

BUILD ?= build/$(TARGET_OS)-$(TOOLCHAIN)
BIN   := $(BUILD)/$(NAME)$(EXE)

# ---------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------
ifneq (,$(filter default undefined,$(origin CC)))
  ifeq ($(TOOLCHAIN),msvc)
    CC := cl
  else ifeq ($(IS_CROSS),1)
    CC := $(TRIPLE)-gcc
  else ifeq ($(TOOLCHAIN),clang)
    CC := clang
  else ifneq ($(filter macos bsd,$(TARGET_OS)),)
    # Apple clang / base-system clang is the native compiler there.
    CC := $(if $(shell command -v clang 2>/dev/null),clang,cc)
  else
    CC := $(if $(shell command -v gcc 2>/dev/null),gcc,$(if $(shell command -v clang 2>/dev/null),clang,cc))
  endif
endif

ifeq ($(IS_CROSS),1)
  WINDRES ?= $(TRIPLE)-windres
else
  WINDRES ?= $(if $(shell command -v windres 2>/dev/null),windres,llvm-windres)
endif

PKG_CONFIG ?= pkg-config
GLIB_COMPILE_RESOURCES ?= glib-compile-resources

# Cross-compiling for Windows from Linux: prefer <triple>-pkg-config (Fedora/MXE),
# otherwise point pkg-config at the MinGW sysroot (Arch/Debian/Ubuntu layouts).
ifeq ($(IS_CROSS),1)
  CROSS_PC := $(shell command -v $(TRIPLE)-pkg-config 2>/dev/null)
  ifneq ($(CROSS_PC),)
    PKG_CONFIG := $(CROSS_PC)
  else
    MINGW_PREFIX ?= /usr/$(TRIPLE)
    export PKG_CONFIG_LIBDIR := $(MINGW_PREFIX)/lib/pkgconfig:$(MINGW_PREFIX)/share/pkgconfig
  endif
endif

# Homebrew (Apple Silicon /opt/homebrew, Intel /usr/local) and BSD ports
# (/usr/local) usually export pkg-config paths themselves; add fallbacks.
ifeq ($(TARGET_OS),macos)
  BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
  ifneq ($(BREW_PREFIX),)
    export PKG_CONFIG_PATH := $(BREW_PREFIX)/lib/pkgconfig:$(BREW_PREFIX)/share/pkgconfig:$(PKG_CONFIG_PATH)
  endif
endif
ifeq ($(TARGET_OS),bsd)
  export PKG_CONFIG_PATH := /usr/local/lib/pkgconfig:/usr/local/libdata/pkgconfig:$(PKG_CONFIG_PATH)
endif

# Prebuilt GTK4 tree (e.g. gvsbuild output for MSVC).
ifdef GTK_PREFIX
  export PKG_CONFIG_PATH := $(GTK_PREFIX)/lib/pkgconfig
endif

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
PKGS := gtk4
DEFS :=

ifneq ($(filter linux bsd,$(TARGET_OS)),)
  ifneq ($(LAYER_SHELL),0)
    HAVE_LAYER_SHELL := $(shell $(PKG_CONFIG) --exists gtk4-layer-shell-0 2>/dev/null && echo 1)
    ifeq ($(HAVE_LAYER_SHELL),1)
      PKGS += gtk4-layer-shell-0
      DEFS += -DLH_HAVE_LAYER_SHELL
    else ifeq ($(LAYER_SHELL),1)
      $(error LAYER_SHELL=1 but gtk4-layer-shell-0 was not found by pkg-config)
    endif
  endif
endif

PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKGS) 2>/dev/null)
ifeq ($(TOOLCHAIN),msvc)
  # cl/link do not understand -L/-l; translate pkg-config's GNU-style output.
  PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PKGS) 2>/dev/null | sed -e 's/\(^\| \)-L/\1-LIBPATH:/g' -e 's/\(^\| \)-l\([^ ]*\)/\1\2.lib/g')
else
  PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PKGS) 2>/dev/null) -lm
endif

# ---------------------------------------------------------------------------
# Flags
# ---------------------------------------------------------------------------
ifeq ($(TOOLCHAIN),msvc)
  CFLAGS_ALL := -nologo -TC -std:c11 -utf-8 -W3 -MD -D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN \
                $(if $(filter 1,$(DEBUG)),-Od -Zi,-O2) $(DEFS) -Isrc $(PC_CFLAGS) $(CFLAGS)
  PC_LIBS += shell32.lib
  LDFLAGS_ALL := -nologo $(if $(filter 1,$(DEBUG)),-DEBUG,) \
                 $(if $(filter 1,$(CONSOLE)),-SUBSYSTEM:CONSOLE,-SUBSYSTEM:WINDOWS -ENTRY:mainCRTStartup) $(LDFLAGS)
else
  CFLAGS_ALL := -std=gnu11 -Wall -Wextra -Wformat=2 -Werror=format-security -pipe -fno-strict-aliasing -MMD -MP \
                $(if $(filter 1,$(DEBUG)),-O0 -g,-O2) $(if $(MARCH),-march=$(MARCH),) $(if $(filter 1,$(LTO)),-flto,) \
                $(DEFS) -Isrc $(PC_CFLAGS)
  LDFLAGS_ALL := $(if $(filter 1,$(LTO)),-flto,)
  ifeq ($(TARGET_OS),linux)
    LDFLAGS_ALL += -Wl,--as-needed
    ifeq ($(DEBUG),0)
      CFLAGS_ALL  += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE
      LDFLAGS_ALL += -pie -Wl,-z,relro,-z,now
    endif
  endif
  ifeq ($(TARGET_OS),macos)
    LDFLAGS_ALL += -Wl,-dead_strip
  endif
  ifeq ($(TARGET_OS),windows)
    LDFLAGS_ALL += $(if $(filter 1,$(CONSOLE)),,-mwindows)
    PC_LIBS += -lshell32
  endif
  CFLAGS_ALL  += $(CFLAGS)
  LDFLAGS_ALL += $(LDFLAGS)
endif

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
SRC := src/main.c src/platform.c src/prefs.c src/theme.c src/desktop.c src/panel.c src/sidebar.c \
       src/start_menu.c src/settings.c src/explorer.c src/appstore.c \
       src/LH4051-WM/wm.c src/LH4051-FM/fm.c src/LH4051-SESSION/session.c
OBJS    := $(patsubst %.c,$(BUILD)/%$(OBJEXT),$(SRC))
RES_C   := $(BUILD)/resources.c
RES_OBJ := $(BUILD)/resources$(OBJEXT)
ALL_OBJS := $(OBJS) $(RES_OBJ)

ifeq ($(TARGET_OS),windows)
  ifeq ($(TOOLCHAIN),msvc)
    WIN_RES := $(BUILD)/lh4051-de.res
  else
    WIN_RES := $(BUILD)/lh4051-de.res.o
  endif
  ALL_OBJS += $(WIN_RES)
endif

comma := ,

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
.PHONY: all linux macos bsd win64 msvc run clean distclean check-deps show-config help \
        install uninstall sessions dist-linux dist-unix dist-macos dist-win64 dist-arch dist-arch-git dist-stable dist \
        installer-unix installer-macos installer-win

all: $(BIN)

linux:
	@$(MAKE) --no-print-directory TARGET_OS=linux TOOLCHAIN=gcc all

macos:
	@$(MAKE) --no-print-directory TARGET_OS=macos TOOLCHAIN=clang all

bsd:
	@$(MAKE) --no-print-directory TARGET_OS=bsd all

win64:
	@$(MAKE) --no-print-directory TARGET_OS=windows TOOLCHAIN=mingw all

msvc:
	@$(MAKE) --no-print-directory TARGET_OS=windows TOOLCHAIN=msvc all

check-deps:
	@$(PKG_CONFIG) --exists gtk4 || { \
	  echo "error: GTK4 development files were not found by '$(PKG_CONFIG)'."; \
	  echo "  Debian/Ubuntu: sudo apt install libgtk-4-dev libglib2.0-dev-bin [libgtk4-layer-shell-dev]"; \
	  echo "  Fedora:        sudo dnf install gtk4-devel gtk4-layer-shell-devel"; \
	  echo "  Arch:          sudo pacman -S gtk4 gtk4-layer-shell"; \
	  echo "  Alpine:        apk add gtk4.0-dev glib-dev build-base pkgconf"; \
	  echo "  macOS:         brew install gtk4 pkgconf"; \
	  echo "  FreeBSD:       pkg install gtk4 gmake pkgconf   (OpenBSD: pkg_add gtk4 gmake)"; \
	  echo "  MSYS2 UCRT64:  pacman -S mingw-w64-ucrt-x86_64-gtk4 mingw-w64-ucrt-x86_64-toolchain"; \
	  echo "  MSVC:          set GTK_PREFIX to a gvsbuild GTK4 tree"; \
	  exit 1; }
	@command -v $(GLIB_COMPILE_RESOURCES) >/dev/null 2>&1 || { echo "error: $(GLIB_COMPILE_RESOURCES) not found (package libglib2.0-dev-bin / glib2-devel)."; exit 1; }
	@command -v $(firstword $(CC)) >/dev/null 2>&1 || { echo "error: compiler '$(firstword $(CC))' not found."; exit 1; }
ifeq ($(TOOLCHAIN),msvc)
	@link -? 2>&1 | grep -qi microsoft || { echo "error: MSVC link.exe not first in PATH (MSYS2's coreutils 'link' shadows it). Use a VS developer prompt."; exit 1; }
endif

$(BIN): $(ALL_OBJS)
	@mkdir -p $(@D)
	@printf '==> Linking %s %s [%s/%s]\n' "$(NAME)" "$(VERSION)" "$(TARGET_OS)" "$(TOOLCHAIN)"
ifeq ($(TOOLCHAIN),msvc)
	@link $(LDFLAGS_ALL) -OUT:$@ $(ALL_OBJS) $(PC_LIBS)
else
	@$(CC) $(ALL_OBJS) -o $@ $(LDFLAGS_ALL) $(PC_LIBS)
endif

$(BUILD)/%$(OBJEXT): %.c | check-deps
	@mkdir -p $(@D)
	@printf 'CC  %s\n' "$<"
ifeq ($(TOOLCHAIN),msvc)
	@$(CC) $(CFLAGS_ALL) -c $< -Fo$@
else
	@$(CC) $(CFLAGS_ALL) -c $< -o $@
endif

$(RES_C): data/lh4051-de.gresource.xml data/theme.css images/wallpaper.jpg | check-deps
	@mkdir -p $(@D)
	@printf 'RES %s\n' "$@"
	@$(GLIB_COMPILE_RESOURCES) --sourcedir=data --target=$@ --generate-source $<

$(RES_OBJ): $(RES_C)
ifeq ($(TOOLCHAIN),msvc)
	@$(CC) $(CFLAGS_ALL) -c $< -Fo$@
else
	@$(CC) $(CFLAGS_ALL) -c $< -o $@
endif

$(BUILD)/version.h: src/longhorn.h
	@mkdir -p $(@D)
	@printf '#define LH_RC_VERSION %s,0\n#define LH_RC_VERSION_STR "%s"\n' "$(subst .,$(comma),$(VERSION))" "$(VERSION)" > $@

ifeq ($(TARGET_OS),windows)
ifeq ($(TOOLCHAIN),msvc)
$(WIN_RES): windows/lh4051-de.rc windows/lh4051-de.manifest $(BUILD)/version.h
	@printf 'RC  %s\n' "$<"
	@rc -nologo -I$(BUILD) -Iwindows -Fo$@ $<
else
$(WIN_RES): windows/lh4051-de.rc windows/lh4051-de.manifest $(BUILD)/version.h
	@printf 'RC  %s\n' "$<"
	@$(WINDRES) -O coff -I$(BUILD) -Iwindows -i $< -o $@
endif
endif

ifneq ($(TOOLCHAIN),msvc)
-include $(OBJS:$(OBJEXT)=.d)
endif

run: all
	@./$(BIN) $(ARGS)

clean:
	@rm -rf build dist

distclean: clean
	@rm -f PKGBUILD

show-config:
	@echo "version:    $(VERSION)"
	@echo "host/target: $(HOST_OS) -> $(TARGET_OS) ($(TOOLCHAIN)$(if $(IS_CROSS), cross,))"
	@echo "compiler:   $(CC)"
	@echo "pkg-config: $(PKG_CONFIG)  packages: $(PKGS)"
	@echo "layer-shell: $(if $(findstring LH_HAVE_LAYER_SHELL,$(DEFS)),yes,no)"
	@echo "output:     $(BIN)"
	@echo "cflags:     $(CFLAGS_ALL)"
	@echo "ldflags:    $(LDFLAGS_ALL) $(PC_LIBS)"

# ---------------------------------------------------------------------------
# Install (unix-like) + session files
# ---------------------------------------------------------------------------
SESSION_DIR := $(BUILD)/data
sessions: $(SESSION_DIR)/wayland-sessions/lh4051-de.desktop $(SESSION_DIR)/xsessions/lh4051-de.desktop

$(SESSION_DIR)/%/lh4051-de.desktop: data/%/lh4051-de.desktop.in
	@mkdir -p $(@D)
	@sed -e 's|@bindir@|$(PREFIX)/bin|g' -e 's|@version@|$(VERSION)|g' $< > $@

INSTALL_FILE = mkdir -p "$(dir $(2))" && install -m $(1) "$(3)" "$(2)"
APPS_SRC := data/applications/lh4051-de.desktop.in
APPS_OUT := $(SESSION_DIR)/applications/lh4051-de.desktop

$(APPS_OUT): $(APPS_SRC)
	@mkdir -p $(@D)
	@sed -e 's|@bindir@|$(PREFIX)/bin|g' -e 's|@version@|$(VERSION)|g' $< > $@

install: all sessions $(APPS_OUT)
ifeq ($(UNIX_LIKE),)
	@echo "error: 'make install' is for unix-like systems. For Windows use 'make dist-win64' or 'make installer-win'."; exit 1
else
	$(call INSTALL_FILE,755,$(DESTDIR)$(PREFIX)/bin/$(NAME),$(BIN))
	$(call INSTALL_FILE,755,$(DESTDIR)$(PREFIX)/bin/lh4051-de-session,scripts/lh4051-de-session)
	$(call INSTALL_FILE,644,$(DESTDIR)$(PREFIX)/share/applications/lh4051-de.desktop,$(APPS_OUT))
	$(call INSTALL_FILE,644,$(DESTDIR)$(PREFIX)/share/wayland-sessions/lh4051-de.desktop,$(SESSION_DIR)/wayland-sessions/lh4051-de.desktop)
	$(call INSTALL_FILE,644,$(DESTDIR)$(PREFIX)/share/xsessions/lh4051-de.desktop,$(SESSION_DIR)/xsessions/lh4051-de.desktop)
endif

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(NAME) $(DESTDIR)$(PREFIX)/bin/lh4051-de-session \
	  $(DESTDIR)$(PREFIX)/share/applications/lh4051-de.desktop \
	  $(DESTDIR)$(PREFIX)/share/wayland-sessions/lh4051-de.desktop $(DESTDIR)$(PREFIX)/share/xsessions/lh4051-de.desktop

# ---------------------------------------------------------------------------
# Distribution artifacts
# ---------------------------------------------------------------------------
# Generic unix tarball (Linux, BSD, ...): dist/lh4051-de-<ver>-<os>-<arch>.tar.gz
dist-unix:
	@rm -rf dist/stage-unix && mkdir -p dist
	@$(MAKE) --no-print-directory PREFIX=/usr DESTDIR=$(CURDIR)/dist/stage-unix install
	@tar -C dist/stage-unix -czf dist/$(NAME)-$(VERSION)-$(TARGET_OS)-$(DIST_ARCH).tar.gz .
	@echo "==> dist/$(NAME)-$(VERSION)-$(TARGET_OS)-$(DIST_ARCH).tar.gz"

dist-linux:
	@$(MAKE) --no-print-directory TARGET_OS=linux TOOLCHAIN=gcc dist-unix

# macOS: LH4051-DE.app (zip) built from the native binary.
APP_DIR := dist/LH4051-DE.app
dist-macos:
	@$(MAKE) --no-print-directory TARGET_OS=macos TOOLCHAIN=clang all
	@rm -rf $(APP_DIR) && mkdir -p $(APP_DIR)/Contents/MacOS $(APP_DIR)/Contents/Resources
	@cp build/macos-clang/$(NAME) $(APP_DIR)/Contents/MacOS/$(NAME)-bin
	@cp scripts/macos-launcher.sh $(APP_DIR)/Contents/MacOS/LH4051-DE && chmod 755 $(APP_DIR)/Contents/MacOS/LH4051-DE
	@sed -e 's|@version@|$(VERSION)|g' macos/Info.plist.in > $(APP_DIR)/Contents/Info.plist
	@cd dist && ditto -c -k --keepParent LH4051-DE.app $(NAME)-$(VERSION)-macos-$(DIST_ARCH).zip 2>/dev/null || \
	  (cd dist && zip -qr $(NAME)-$(VERSION)-macos-$(DIST_ARCH).zip LH4051-DE.app)
	@echo "==> dist/$(NAME)-$(VERSION)-macos-$(DIST_ARCH).zip"

# "LH4051-DE Installer" for unix-like systems: self-extracting .run
installer-unix:
	@$(MAKE) --no-print-directory dist-unix
	@sh scripts/make-installer-unix.sh "dist/$(NAME)-$(VERSION)-$(TARGET_OS)-$(DIST_ARCH).tar.gz" \
	  "dist/LH4051-DE-Installer-$(VERSION)-$(TARGET_OS)-$(DIST_ARCH).run" "$(VERSION)"

# "LH4051-DE Installer" for macOS: .pkg installing LH4051-DE.app into /Applications
installer-macos: dist-macos
	@pkgbuild --component $(APP_DIR) --install-location /Applications --identifier org.kuznix.lh4051de \
	  --version $(VERSION) "dist/LH4051-DE-Installer-$(VERSION)-macos-$(DIST_ARCH).pkg"
	@echo "==> dist/LH4051-DE-Installer-$(VERSION)-macos-$(DIST_ARCH).pkg"

# "LH4051-DE Installer" for Windows (Inno Setup, run after dist-win64)
installer-win:
	@iscc /Qp /DAppVersion=$(VERSION) /DArch=$(WIN_ARCH) \
	  "/DSourceDir=$(CURDIR)/dist/$(NAME)-$(VERSION)-win-$(WIN_ARCH)" "/DOutDir=$(CURDIR)/dist" installer/lh4051-de.iss

dist-win64:
	@$(MAKE) --no-print-directory win64
	@sh scripts/bundle-win.sh "build/windows-mingw/$(NAME).exe" "dist/$(NAME)-$(VERSION)-win-$(WIN_ARCH)" "$(TRIPLE)"

# Arch package, stable release (needs the v$(VERSION) tag on the remote).
dist-arch dist-stable:
	@command -v makepkg >/dev/null 2>&1 && command -v pacman >/dev/null 2>&1 || { echo "dist-arch needs pacman/makepkg"; exit 2; }
	@sed -e "s/@PKGVER@/$(VERSION)/g" -e "s/@ARCH@/$(shell uname -m)/g" PKGBUILD.in > PKGBUILD
	@makepkg -f

# Arch package from the current git HEAD (development snapshot).
dist-arch-git:
	@command -v makepkg >/dev/null 2>&1 && command -v pacman >/dev/null 2>&1 || { echo "dist-arch-git needs pacman/makepkg"; exit 2; }
	@sed -e "s/@PKGVER@/0.0.0/g" -e "s/@ARCH@/$(shell uname -m)/g" PKGBUILD-git.in > PKGBUILD
	@makepkg -f

dist: dist-arch

help:
	@echo "LH4051-DE $(VERSION)"
	@echo ""
	@echo "Targets"
	@echo "  make / make linux    build for the host OS (Linux, macOS, BSD via gmake)"
	@echo "  make macos           macOS (Intel or Apple Silicon, clang + Homebrew GTK4)"
	@echo "  make win64           MinGW-w64 build -> build/windows-mingw/lh4051-de.exe"
	@echo "                       (MSYS2 MINGW64 shell, or cross: apt install gcc-mingw-w64-x86-64 + MinGW GTK4)"
	@echo "  make msvc            MSVC build; needs GTK_PREFIX=<gvsbuild tree> and a VS developer prompt"
	@echo "  make run [ARGS=-w]   build and run"
	@echo "  make install         install to PREFIX (default /usr/local; DESTDIR supported)"
	@echo "  make dist-unix       dist/lh4051-de-<ver>-<os>-<arch>.tar.gz (Linux, BSD, ...)"
	@echo "  make dist-macos      dist/LH4051-DE.app + zip"
	@echo "  make installer-unix  LH4051-DE Installer (.run) for Linux/BSD/unix"
	@echo "  make installer-macos LH4051-DE Installer (.pkg) for macOS"
	@echo "  make installer-win   LH4051-DE Installer (.exe, Inno Setup) after dist-win64"
	@echo "  make dist-win64      dist/lh4051-de-<ver>-win-<arch>/ + .zip with bundled GTK runtime"
	@echo "  make dist-arch       Arch package (stable tag) via makepkg; dist-arch-git for HEAD"
	@echo "  make show-config     print the resolved toolchain/flags"
	@echo "  make clean"
	@echo ""
	@echo "Variables: DEBUG=1 LTO=1 MARCH=x86-64-v3 CONSOLE=1 LAYER_SHELL=auto|1|0 PREFIX= DESTDIR="
	@echo "           CC= TRIPLE= MINGW_PREFIX= GTK_PREFIX= BUILD="
