# LH4051-DE — GNU make build system
#
#   Linux (gcc/clang)        make
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
  HOST_OS := $(if $(filter Linux,$(shell uname -s 2>/dev/null)),linux,other)
endif

TARGET_OS ?= $(HOST_OS)
# TOOLCHAIN: gcc | clang | mingw | msvc
TOOLCHAIN ?= $(if $(filter windows,$(TARGET_OS)),mingw,gcc)
TRIPLE    ?= x86_64-w64-mingw32
IS_CROSS  := $(if $(and $(filter-out windows,$(HOST_OS)),$(filter windows,$(TARGET_OS))),1,)

# DEBUG=1: -O0 -g, no hardening.  LTO=1: link-time optimisation (gcc/clang).
# MARCH: e.g. x86-64-v3 or native.  CONSOLE=1: keep a console window on Windows.
# LAYER_SHELL: auto | 1 | 0  (Linux only: gtk4-layer-shell support).
DEBUG   ?= 0
LTO     ?= 0
MARCH   ?=
CONSOLE ?= 0
LAYER_SHELL ?= auto
# WIN_ARCH only names the Windows bundle (x64, x86, arm64).
WIN_ARCH ?= x64

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
  else
    CC := $(if $(shell command -v gcc 2>/dev/null),gcc,clang)
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

# Prebuilt GTK4 tree (e.g. gvsbuild output for MSVC).
ifdef GTK_PREFIX
  export PKG_CONFIG_PATH := $(GTK_PREFIX)/lib/pkgconfig
endif

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
PKGS := gtk4
DEFS :=

ifeq ($(TARGET_OS),linux)
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
.PHONY: all linux win64 msvc run clean distclean check-deps show-config help \
        install uninstall sessions dist-linux dist-win64 dist-arch dist-stable dist

all: $(BIN)

linux:
	@$(MAKE) --no-print-directory TARGET_OS=linux TOOLCHAIN=gcc all

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
	  echo "  MSYS2 MINGW64: pacman -S mingw-w64-x86_64-gtk4 mingw-w64-x86_64-toolchain mingw-w64-x86_64-pkgconf"; \
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
# Install (Linux) + session files
# ---------------------------------------------------------------------------
SESSION_DIR := $(BUILD)/data
sessions: $(SESSION_DIR)/wayland-sessions/lh4051-de.desktop $(SESSION_DIR)/xsessions/lh4051-de.desktop

$(SESSION_DIR)/%/lh4051-de.desktop: data/%/lh4051-de.desktop.in
	@mkdir -p $(@D)
	@sed -e 's|@bindir@|$(PREFIX)/bin|g' -e 's|@version@|$(VERSION)|g' $< > $@

install: all sessions
ifneq ($(TARGET_OS),linux)
	@echo "error: 'make install' is Linux only. For Windows use 'make dist-win64'."; exit 1
else
	install -Dm755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(NAME)
	install -Dm755 scripts/lh4051-de-session $(DESTDIR)$(PREFIX)/bin/lh4051-de-session
	install -Dm644 $(SESSION_DIR)/wayland-sessions/lh4051-de.desktop $(DESTDIR)$(PREFIX)/share/wayland-sessions/lh4051-de.desktop
	install -Dm644 $(SESSION_DIR)/xsessions/lh4051-de.desktop $(DESTDIR)$(PREFIX)/share/xsessions/lh4051-de.desktop
endif

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(NAME) $(DESTDIR)$(PREFIX)/bin/lh4051-de-session \
	  $(DESTDIR)$(PREFIX)/share/wayland-sessions/lh4051-de.desktop $(DESTDIR)$(PREFIX)/share/xsessions/lh4051-de.desktop

# ---------------------------------------------------------------------------
# Distribution artifacts
# ---------------------------------------------------------------------------
dist-linux:
	@rm -rf dist/stage-linux && mkdir -p dist
	@$(MAKE) --no-print-directory TARGET_OS=linux TOOLCHAIN=gcc PREFIX=/usr DESTDIR=$(CURDIR)/dist/stage-linux install
	@tar -C dist/stage-linux -czf dist/$(NAME)-$(VERSION)-linux-$(shell uname -m).tar.gz .
	@echo "==> dist/$(NAME)-$(VERSION)-linux-$(shell uname -m).tar.gz"

dist-win64:
	@$(MAKE) --no-print-directory win64
	@sh scripts/bundle-win.sh "build/windows-mingw/$(NAME).exe" "dist/$(NAME)-$(VERSION)-win-$(WIN_ARCH)" "$(TRIPLE)"

dist-arch:
	@command -v makepkg >/dev/null 2>&1 && command -v pacman >/dev/null 2>&1 || { echo "dist-arch needs pacman/makepkg"; exit 2; }
	@sed -e "s/@PKGVER@/0.0.0.r0.git/g" -e "s/@ARCH@/$(shell uname -m)/g" PKGBUILD.in > PKGBUILD
	@makepkg -f

dist-stable:
	@command -v makepkg >/dev/null 2>&1 && command -v pacman >/dev/null 2>&1 || { echo "dist-stable needs pacman/makepkg"; exit 2; }
	@sed -e "s/@PKGVER@/$(VERSION)/g" -e "s/@ARCH@/$(shell uname -m)/g" PKGBUILD.in > PKGBUILD
	@LH4051_STABLE_VERSION="$(VERSION)" makepkg -f

dist: dist-arch

help:
	@echo "LH4051-DE $(VERSION)"
	@echo ""
	@echo "Targets"
	@echo "  make / make linux    build for Linux (gcc; CC=clang works)"
	@echo "  make win64           MinGW-w64 build -> build/windows-mingw/lh4051-de.exe"
	@echo "                       (MSYS2 MINGW64 shell, or cross: apt install gcc-mingw-w64-x86-64 + MinGW GTK4)"
	@echo "  make msvc            MSVC build; needs GTK_PREFIX=<gvsbuild tree> and a VS developer prompt"
	@echo "  make run [ARGS=-w]   build and run"
	@echo "  make install         install to PREFIX (default /usr/local; DESTDIR supported)"
	@echo "  make dist-linux      dist/lh4051-de-<ver>-linux-<arch>.tar.gz"
	@echo "  make dist-win64      dist/lh4051-de-<ver>-win-<arch>/ + .zip with bundled GTK runtime"
	@echo "  make dist-arch       Arch package via makepkg"
	@echo "  make show-config     print the resolved toolchain/flags"
	@echo "  make clean"
	@echo ""
	@echo "Variables: DEBUG=1 LTO=1 MARCH=x86-64-v3 CONSOLE=1 LAYER_SHELL=auto|1|0 PREFIX= DESTDIR="
	@echo "           CC= TRIPLE= MINGW_PREFIX= GTK_PREFIX= BUILD="
