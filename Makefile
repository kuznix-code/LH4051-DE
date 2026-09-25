# LH4051-DE Makefile
VERSION := 0.0.0
CC ?= gcc
PKG_CONFIG ?= pkg-config

# Use TARGET=... to select a toolchain explicitly.
# With no TARGET, detect the current host OS/CPU automatically.
TARGET ?= $(shell sh -c 'os=$(uname -s 2>/dev/null | tr "[:upper:]" "[:lower:]"); id=$(. /etc/os-release 2>/dev/null && printf "%s" "$ID"); m=$(uname -m 2>/dev/null); case "$os:$m:$id" in linux:x86_64:cachyos) echo linux-cachy;; linux:x86_64:ubuntu) echo linux-ubuntu-x86_64;; linux:x86_64:debian) echo linux-debian-x86_64;; linux:x86_64:fedora) echo linux-fedora-x86_64;; linux:x86_64:arch) echo linux-arch-x86_64;; linux:x86_64:*) echo linux-generic-x86_64;; linux:aarch64:* ) echo linux-generic-aarch64;; linux:armv7l:*|linux:armv7:* ) echo linux-generic-armv7;; linux:armv6l:*|linux:armv6:* ) echo linux-generic-armv6;; linux:i[3456]86:* ) echo linux-generic-i686;; linux:riscv64:* ) echo linux-generic-riscv64;; linux:ppc64le:* ) echo linux-generic-ppc64el;; linux:ppc64:* ) echo linux-generic-ppc64;; linux:s390x:* ) echo linux-generic-s390x;; linux:mips64*:* ) echo linux-generic-mips64;; linux:mips*:* ) echo linux-generic-mips;; darwin:x86_64:*|darwin:amd64:* ) echo darwin-x86_64;; darwin:arm64:* ) echo darwin-aarch64;; freebsd:x86_64:*|freebsd:amd64:* ) echo freebsd-x86_64;; freebsd:aarch64:*|freebsd:arm64:* ) echo freebsd-aarch64;; *) echo generic;; esac')

RESET := \033[0m
CYAN := \033[1;36m
BLUE := \033[1;34m
GREEN := \033[1;32m
YELLOW := \033[1;33m
RED := \033[1;31m
MAGENTA := \033[1;35m
DIM := \033[2m

GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk4 gtk4-layer-shell-0 2>/dev/null)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk4 gtk4-layer-shell-0 2>/dev/null)

# Target -> compiler/architecture mapping.
# Cross-compilers must be installed separately; the Makefile never downloads them.
TARGET_CC_linux-generic-x86_64 := gcc
TARGET_CC_linux-generic-x86_64v2 := gcc
TARGET_CC_linux-generic-x86_64v3 := gcc
TARGET_CC_linux-generic-x86_64v4 := gcc
TARGET_CC_linux-cachy := gcc
TARGET_CC_linux-ubuntu-x86_64 := x86_64-linux-gnu-gcc
TARGET_CC_linux-ubuntu-x86_64v3 := x86_64-linux-gnu-gcc
TARGET_CC_linux-debian-x86_64 := x86_64-linux-gnu-gcc
TARGET_CC_linux-fedora-x86_64 := x86_64-linux-gnu-gcc
TARGET_CC_linux-arch-x86_64 := x86_64-linux-gnu-gcc
TARGET_CC_linux-alhp-v2 := x86_64-linux-gnu-gcc
TARGET_CC_linux-alhp-v3 := x86_64-linux-gnu-gcc
TARGET_CC_linux-alhp-v4 := x86_64-linux-gnu-gcc
TARGET_CC_linux-generic-aarch64 := aarch64-linux-gnu-gcc
TARGET_CC_linux-ubuntu-aarch64 := aarch64-linux-gnu-gcc
TARGET_CC_linux-debian-aarch64 := aarch64-linux-gnu-gcc
TARGET_CC_linux-fedora-aarch64 := aarch64-linux-gnu-gcc
TARGET_CC_linux-arch-aarch64 := aarch64-linux-gnu-gcc
TARGET_CC_linux-generic-armv7 := arm-linux-gnueabihf-gcc
TARGET_CC_linux-ubuntu-armv7 := arm-linux-gnueabihf-gcc
TARGET_CC_linux-debian-armv7 := arm-linux-gnueabihf-gcc
TARGET_CC_linux-arch-armv7 := arm-linux-gnueabihf-gcc
TARGET_CC_linux-generic-armv6 := arm-linux-gnueabihf-gcc
TARGET_CC_linux-debian-armv6 := arm-linux-gnueabihf-gcc
TARGET_CC_linux-generic-armv5 := arm-linux-gnueabi-gcc
TARGET_CC_linux-generic-armv4 := arm-linux-gnueabi-gcc
TARGET_CC_linux-generic-i486 := i486-linux-gnu-gcc
TARGET_CC_linux-generic-i586 := i586-linux-gnu-gcc
TARGET_CC_linux-generic-i686 := i686-linux-gnu-gcc
TARGET_CC_linux-generic-i386 := i386-linux-gnu-gcc
TARGET_CC_linux-arch-i486 := i486-linux-gnu-gcc
TARGET_CC_linux-arch-i686 := i686-linux-gnu-gcc
TARGET_CC_linux-debian-i686 := i686-linux-gnu-gcc
TARGET_CC_linux-generic-mips := mips-linux-gnu-gcc
TARGET_CC_linux-generic-mipsel := mipsel-linux-gnu-gcc
TARGET_CC_linux-generic-mips64 := mips64-linux-gnuabi64-gcc
TARGET_CC_linux-generic-mips64el := mips64el-linux-gnuabi64-gcc
TARGET_CC_linux-debian-mips := mips-linux-gnu-gcc
TARGET_CC_linux-debian-mipsel := mipsel-linux-gnu-gcc
TARGET_CC_linux-debian-mips64el := mips64el-linux-gnuabi64-gcc
TARGET_CC_linux-generic-ppc := powerpc-linux-gnu-gcc
TARGET_CC_linux-generic-ppc64 := powerpc64-linux-gnu-gcc
TARGET_CC_linux-generic-ppc64el := powerpc64le-linux-gnu-gcc
TARGET_CC_linux-generic-ppc64el-power8 := powerpc64le-linux-gnu-gcc
TARGET_CC_linux-generic-ppc64el-power9 := powerpc64le-linux-gnu-gcc
TARGET_CC_linux-debian-ppc := powerpc-linux-gnu-gcc
TARGET_CC_linux-debian-ppc64 := powerpc64-linux-gnu-gcc
TARGET_CC_linux-debian-ppc64el := powerpc64le-linux-gnu-gcc
TARGET_CC_linux-generic-sh := sh-linux-gnu-gcc
TARGET_CC_linux-generic-sh2 := sh-linux-gnu-gcc
TARGET_CC_linux-generic-sh3 := sh-linux-gnu-gcc
TARGET_CC_linux-generic-sh4 := sh-linux-gnu-gcc
TARGET_CC_linux-generic-sh5 := sh-linux-gnu-gcc
TARGET_CC_linux-generic-x32 := x86_64-linux-gnu-gcc
TARGET_CC_linux-generic-s390 := s390x-linux-gnu-gcc
TARGET_CC_linux-generic-s390x := s390x-linux-gnu-gcc
TARGET_CC_linux-ubuntu-s390x := s390x-linux-gnu-gcc
TARGET_CC_linux-debian-s390x := s390x-linux-gnu-gcc
TARGET_CC_linux-fedora-s390x := s390x-linux-gnu-gcc
TARGET_CC_linux-generic-i686-pentium4 := i686-linux-gnu-gcc
TARGET_CC_linux-generic-m32r := m32r-linux-gnu-gcc
TARGET_CC_linux-generic-m68k := m68k-linux-gnu-gcc
TARGET_CC_linux-generic-alpha := alpha-linux-gnu-gcc
TARGET_CC_linux-generic-or1k := or1k-linux-gnu-gcc
TARGET_CC_linux-generic-ia64 := ia64-linux-gnu-gcc
TARGET_CC_linux-generic-mn10300 := mn10300-linux-gnu-gcc
TARGET_CC_linux-generic-ppc-ps3 := powerpc64-linux-gnu-gcc
TARGET_CC_linux-generic-loong32 := loongarch32-linux-gnu-gcc
TARGET_CC_linux-generic-loong64 := loongarch64-linux-gnu-gcc
TARGET_CC_linux-debian-loong64 := loongarch64-linux-gnu-gcc
TARGET_CC_linux-arch-loong64 := loongarch64-linux-gnu-gcc
TARGET_CC_linux-generic-riscv32 := riscv32-linux-gnu-gcc
TARGET_CC_linux-generic-riscv64 := riscv64-linux-gnu-gcc
TARGET_CC_linux-ubuntu-riscv64 := riscv64-linux-gnu-gcc
TARGET_CC_linux-debian-riscv64 := riscv64-linux-gnu-gcc
TARGET_CC_linux-fedora-riscv64 := riscv64-linux-gnu-gcc
TARGET_CC_linux-arch-riscv64 := riscv64-linux-gnu-gcc
TARGET_CC_linux-generic-hppa-1.0 := hppa-linux-gnu-gcc
TARGET_CC_linux-generic-hppa-1.1 := hppa-linux-gnu-gcc

TARGET_CC_win7-x86_64 := x86_64-w64-mingw32-gcc
TARGET_CC_win8-x86_64 := x86_64-w64-mingw32-gcc
TARGET_CC_win8.1-x86_64 := x86_64-w64-mingw32-gcc
TARGET_CC_win10-x86_64 := x86_64-w64-mingw32-gcc
TARGET_CC_win11-x86_64 := x86_64-w64-mingw32-gcc
TARGET_CC_win7-i686 := i686-w64-mingw32-gcc
TARGET_CC_win8-i686 := i686-w64-mingw32-gcc
TARGET_CC_win8.1-i686 := i686-w64-mingw32-gcc
TARGET_CC_win10-i686 := i686-w64-mingw32-gcc
TARGET_CC_win8-aarch64 := aarch64-w64-mingw32-gcc
TARGET_CC_win8.1-aarch64 := aarch64-w64-mingw32-gcc
TARGET_CC_win10-aarch64 := aarch64-w64-mingw32-gcc
TARGET_CC_win11-aarch64 := aarch64-w64-mingw32-gcc
TARGET_CC_win8-armv7 := arm-w64-mingw32-gcc
TARGET_CC_win8.1-armv7 := arm-w64-mingw32-gcc
TARGET_CC_win10-armv7 := arm-w64-mingw32-gcc

TARGET_CC_darwin-x86_64 := x86_64-apple-darwin-clang
TARGET_CC_darwin-i686 := i686-apple-darwin-clang
TARGET_CC_darwin-aarch64 := aarch64-apple-darwin-clang
TARGET_CC_darwin-ppc64 := powerpc64-apple-darwin-gcc
TARGET_CC_darwin-ppc := powerpc-apple-darwin-gcc

TARGET_CC_freebsd-x86_64 := x86_64-unknown-freebsd-gcc
TARGET_CC_freebsd-aarch64 := aarch64-unknown-freebsd-gcc
TARGET_CC_freebsd-i686 := i686-unknown-freebsd-gcc

TARGET_CC_generic := gcc

# x86 tuning. These are compiler options, not claims about distro packaging.
TARGET_CFLAGS_linux-generic-x86_64 := -march=x86-64
TARGET_CFLAGS_linux-generic-x86_64v2 := -march=x86-64-v2
TARGET_CFLAGS_linux-generic-x86_64v3 := -march=x86-64-v3
TARGET_CFLAGS_linux-generic-x86_64v4 := -march=x86-64-v4
TARGET_CFLAGS_linux-cachy := -march=x86-64
TARGET_CFLAGS_linux-alhp-v2 := -march=x86-64-v2
TARGET_CFLAGS_linux-alhp-v3 := -march=x86-64-v3
TARGET_CFLAGS_linux-alhp-v4 := -march=x86-64-v4
TARGET_CFLAGS_linux-generic-i686-pentium4 := -march=pentium4
TARGET_CFLAGS_linux-generic-x32 := -mx32
TARGET_CFLAGS_linux-generic-i486 := -march=i486
TARGET_CFLAGS_linux-generic-i586 := -march=i586
TARGET_CFLAGS_linux-generic-i686 := -march=i686
TARGET_CFLAGS_linux-arch-i486 := -march=i486
TARGET_CFLAGS_linux-arch-i686 := -march=i686

# Known CPU-name aliases. GCC/Clang decides whether a CPU name is valid.
X86_CPU_CODENAMES := bonnell silvermont goldmont goldmont-plus tremont gracemont sandybridge ivybridge haswell broadwell skylake skylake-avx512 cannonlake icelake-client tigerlake alderlake raptorlake meteorlake arrowlake znver1 znver2 znver3 znver4 znver5 znver6

define cpu_target_rules
TARGET_CC_linux-generic-x86_64-$(1) := gcc
TARGET_CFLAGS_linux-generic-x86_64-$(1) := -march=$(1)
endef
$(foreach cpu,$(X86_CPU_CODENAMES),$(eval $(call cpu_target_rules,$(cpu))))

LINUX_TARGETS := \
linux-generic-x86_64 linux-generic-x86_64v2 linux-generic-x86_64v3 linux-generic-x86_64v4 \
linux-cachy linux-ubuntu-x86_64 linux-ubuntu-x86_64v3 linux-debian-x86_64 linux-fedora-x86_64 linux-arch-x86_64 \
linux-alhp-v2 linux-alhp-v3 linux-alhp-v4 linux-generic-aarch64 linux-ubuntu-aarch64 linux-debian-aarch64 linux-fedora-aarch64 linux-arch-aarch64 \
linux-generic-armv7 linux-ubuntu-armv7 linux-debian-armv7 linux-arch-armv7 linux-generic-armv6 linux-debian-armv6 linux-generic-armv5 linux-generic-armv4 \
linux-generic-i486 linux-generic-i586 linux-generic-i686 linux-generic-i386 linux-arch-i486 linux-arch-i686 linux-debian-i686 \
linux-generic-mips linux-generic-mipsel linux-generic-mips64 linux-generic-mips64el linux-debian-mips linux-debian-mipsel linux-debian-mips64el \
linux-generic-ppc linux-generic-ppc64 linux-generic-ppc64el linux-generic-ppc64el-power8 linux-generic-ppc64el-power9 linux-debian-ppc linux-debian-ppc64 linux-debian-ppc64el \
linux-generic-sh linux-generic-sh2 linux-generic-sh3 linux-generic-sh4 linux-generic-sh5 linux-generic-x32 linux-generic-s390 linux-generic-s390x \
linux-ubuntu-s390x linux-debian-s390x linux-fedora-s390x linux-generic-i686-pentium4 linux-generic-m32r linux-generic-m68k linux-generic-alpha linux-generic-or1k linux-generic-ia64 linux-generic-mn10300 \
linux-generic-ppc-ps3 linux-generic-loong32 linux-generic-loong64 linux-debian-loong64 linux-arch-loong64 linux-generic-riscv32 linux-generic-riscv64 \
linux-ubuntu-riscv64 linux-debian-riscv64 linux-fedora-riscv64 linux-arch-riscv64 linux-generic-hppa-1.0 linux-generic-hppa-1.1 $(LINUX_CPU_TARGETS)
WINDOWS_TARGETS := win7-x86_64 win8-x86_64 win8.1-x86_64 win10-x86_64 win11-x86_64 win7-i686 win8-i686 win8.1-i686 win10-i686 win8-aarch64 win8.1-aarch64 win10-aarch64 win11-aarch64 win8-armv7 win8.1-armv7 win10-armv7
DARWIN_TARGETS := darwin-x86_64 darwin-i686 darwin-aarch64 darwin-ppc64 darwin-ppc
FREEBSD_TARGETS := freebsd-x86_64 freebsd-aarch64 freebsd-i686
GENERIC_TARGETS := generic generic-x86_64 generic-aarch64 generic-armv7 generic-armv6 generic-i686 generic-mips generic-mipsel generic-mips64 generic-mips64el generic-ppc generic-ppc64 generic-ppc64el generic-s390x generic-riscv32 generic-riscv64 generic-loong64\nNETBSD_TARGETS := netbsd-x86_64 netbsd-aarch64 netbsd-i686\nOPENBSD_TARGETS := openbsd-x86_64 openbsd-aarch64 openbsd-i686\nDRAGONFLY_TARGETS := dragonfly-x86_64

TARGETS := $(LINUX_TARGETS) $(WINDOWS_TARGETS) $(DARWIN_TARGETS) $(FREEBSD_TARGETS) $(NETBSD_TARGETS) $(OPENBSD_TARGETS) $(DRAGONFLY_TARGETS) $(GENERIC_TARGETS)

# Default build uses the selected target.
CC_SELECTED := $(or $(TARGET_CC_$(TARGET)),$(CC))
TARGET_CFLAGS := $(TARGET_CFLAGS_$(TARGET))

BASE_CFLAGS := -Wall -Wextra -O3 -pipe -DVERSION=\"$(VERSION)\"
CFLAGS := $(BASE_CFLAGS) $(TARGET_CFLAGS) $(GTK_CFLAGS) -Isrc
LDFLAGS := $(GTK_LIBS) -lm

CORE_SRC := src/main.c src/desktop.c src/panel.c src/sidebar.c src/start_menu.c src/explorer.c
CORE_OBJDIR := build/core
CORE_OBJ := $(patsubst src/%.c,$(CORE_OBJDIR)/%.o,$(CORE_SRC))
WM_OBJ := src/LH4051-WM/build/wm.o
FM_OBJ := src/LH4051-FM/build/fm.o
SESSION_OBJ := src/LH4051-SESSION/build/session.o
SUBPROJECT_OBJS := $(WM_OBJ) $(FM_OBJ) $(SESSION_OBJ)
TARGET_BIN := build/lh4051-de

.PHONY: all build clean run help show-target subprojects wm fm session $(TARGETS)

all: $(TARGET_BIN)

build: all

show-target:
	@printf "$(CYAN)LH4051-DE target:$(RESET) $(GREEN)%s$(RESET)\n" "$(TARGET)"
	@printf "  compiler: %s\n" "$(CC_SELECTED)"
	@printf "  cflags:   %s\n" "$(TARGET_CFLAGS)"

$(TARGETS):
	@$(MAKE) --no-print-directory TARGET="$@" all

$(TARGET_BIN): $(CORE_OBJ) $(SUBPROJECT_OBJS)
	@mkdir -p $(@D)
	@printf "$(GREEN)==> Linking LH4051-DE $(VERSION) [$(TARGET)]$(RESET)\n"
	@$(CC_SELECTED) $(CORE_OBJ) $(SUBPROJECT_OBJS) -o $@ $(LDFLAGS)

$(CORE_OBJDIR)/%.o: src/%.c
	@mkdir -p $(@D)
	@printf "$(DIM)CC  %s [%s]\n" "$<" "$(TARGET)"
	@$(CC_SELECTED) $(CFLAGS) -c $< -o $@

$(WM_OBJ):
	@$(MAKE) --no-print-directory -C src/LH4051-WM ARCH="$(TARGET)" VERSION="$(VERSION)" CC="$(CC_SELECTED)" CFLAGS="$(CFLAGS)"
$(FM_OBJ):
	@$(MAKE) --no-print-directory -C src/LH4051-FM ARCH="$(TARGET)" VERSION="$(VERSION)" CC="$(CC_SELECTED)" CFLAGS="$(CFLAGS)"
$(SESSION_OBJ):
	@$(MAKE) --no-print-directory -C src/LH4051-SESSION ARCH="$(TARGET)" VERSION="$(VERSION)" CC="$(CC_SELECTED)" CFLAGS="$(CFLAGS)"

subprojects: $(SUBPROJECT_OBJS)
wm: $(WM_OBJ)
fm: $(FM_OBJ)
session: $(SESSION_OBJ)

clean:
	@printf "$(YELLOW)==> Cleaning build artifacts$(RESET)\n"
	@rm -rf build
	@$(MAKE) --no-print-directory -C src/LH4051-WM clean
	@$(MAKE) --no-print-directory -C src/LH4051-FM clean
	@$(MAKE) --no-print-directory -C src/LH4051-SESSION clean

run: all
	@printf "$(GREEN)==> Running LH4051-DE $(VERSION) [$(TARGET)]$(RESET)\n"
	@./$(TARGET_BIN)

help:
	@printf "$(CYAN)LH4051-DE $(VERSION)$(RESET)\n"
	@printf "  Default target: $(GREEN)$(TARGET)$(RESET) (auto-detected)\n"
	@printf "  Build:          $(GREEN)make$(RESET) or $(GREEN)make TARGET=<target>$(RESET)\n"
	@printf "  Inspect:         $(GREEN)make show-target$(RESET)\n"
	@printf "  Run:             $(GREEN)make run$(RESET)\n"
	@printf "  Clean:           $(GREEN)make clean$(RESET)\n"
	@printf "  Linux targets:  %s\n" "$(LINUX_TARGETS)"
	@printf "  Windows targets:%s\n" "$(WINDOWS_TARGETS)"
	@printf "  macOS targets:  %s\n" "$(DARWIN_TARGETS)"
	@printf "  FreeBSD targets:%s\n" "$(FREEBSD_TARGETS)"
	@printf "  Generic target: $(GENERIC_TARGETS)\n"
	@printf "$(DIM)Cross-compilers/toolchains and target SDKs are not installed by this Makefile. GTK4/gtk4-layer-shell must also exist for native builds.\n$(RESET)"

# Compatibility with the previous ARCH interface.
ifdef ARCH
override TARGET := $(ARCH)
endif
