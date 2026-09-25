# LH4051-DE Makefile
VERSION := 0.0.0
CC ?= gcc
PKG_CONFIG ?= pkg-config
ARCH ?= cachy-v3

RESET := \033[0m
CYAN := \033[1;36m
BLUE := \033[1;34m
GREEN := \033[1;32m
YELLOW := \033[1;33m
MAGENTA := \033[1;35m
DIM := \033[2m

BASE_CFLAGS := -Wall -Wextra -O3 -pipe -DVERSION=\\"$(VERSION)\\"
ifeq ($(ARCH),cachy-v3)
ARCH_CFLAGS := -march=x86-64-v3 -O3 -pipe -fno-plt -fexceptions -Wp,-D_FORTIFY_SOURCE=3 -Wformat -Werror=format-security -fstack-clash-protection -fcf-protection -Wp,-D_GLIBCXX_ASSERTIONS
ARCH_LDFLAGS := -Wl,-O1 -Wl,--sort-common -Wl,--as-needed -Wl,-z,relro -Wl,-z,now -Wl,-z,pack-relative-relocs -flto=auto
else ifeq ($(ARCH),cachy-v4)
ARCH_CFLAGS := -march=x86-64-v4 -O3 -pipe -fno-plt -fexceptions -Wp,-D_FORTIFY_SOURCE=3 -Wformat -Werror=format-security -fstack-clash-protection -fcf-protection -Wp,-D_GLIBCXX_ASSERTIONS
ARCH_LDFLAGS := -Wl,-O1 -Wl,--sort-common -Wl,--as-needed -Wl,-z,relro -Wl,-z,now -Wl,-z,pack-relative-relocs -flto=auto
else
ARCH_CFLAGS := -march=x86-64-v3
ARCH_LDFLAGS :=
endif

GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk4 gtk4-layer-shell-0)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk4 gtk4-layer-shell-0)
CFLAGS := $(BASE_CFLAGS) $(ARCH_CFLAGS) $(GTK_CFLAGS) -Isrc

CORE_SRC := src/main.c src/desktop.c src/panel.c src/sidebar.c src/start_menu.c src/explorer.c
CORE_OBJDIR := build/core
CORE_OBJ := $(patsubst src/%.c,$(CORE_OBJDIR)/%.o,$(CORE_SRC))

WM_OBJ := src/LH4051-WM/build/wm.o
FM_OBJ := src/LH4051-FM/build/fm.o
SESSION_OBJ := src/LH4051-SESSION/build/session.o
SUBPROJECT_OBJS := $(WM_OBJ) $(FM_OBJ) $(SESSION_OBJ)

TARGET := build/lh4051-de
LDFLAGS := $(ARCH_LDFLAGS) $(GTK_LIBS) -lm

.PHONY: all clean run subprojects wm fm session help

all: $(TARGET)

subprojects: $(SUBPROJECT_OBJS)

wm: $(WM_OBJ)
fm: $(FM_OBJ)
session: $(SESSION_OBJ)

$(WM_OBJ):
	@printf "$(CYAN)==> LH4051-WM$(RESET)\n"
	@$(MAKE) --no-print-directory -C src/LH4051-WM ARCH=$(ARCH) VERSION=$(VERSION)

$(FM_OBJ):
	@printf "$(BLUE)==> LH4051-FM$(RESET)\n"
	@$(MAKE) --no-print-directory -C src/LH4051-FM ARCH=$(ARCH) VERSION=$(VERSION)

$(SESSION_OBJ):
	@printf "$(MAGENTA)==> LH4051-SESSION$(RESET)\n"
	@$(MAKE) --no-print-directory -C src/LH4051-SESSION ARCH=$(ARCH) VERSION=$(VERSION)

$(TARGET): $(CORE_OBJ) $(SUBPROJECT_OBJS)
	@mkdir -p $(@D)
	@printf "$(GREEN)==> Linking LH4051-DE $(VERSION) [$(ARCH)]$(RESET)\n"
	@$(CC) $(CORE_OBJ) $(SUBPROJECT_OBJS) -o $@ $(LDFLAGS)

$(CORE_OBJDIR)/%.o: src/%.c
	@mkdir -p $(@D)
	@printf "$(DIM)CC  $<$(RESET)\n"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@printf "$(YELLOW)==> Cleaning build artifacts$(RESET)\n"
	@rm -rf build
	@$(MAKE) --no-print-directory -C src/LH4051-WM clean
	@$(MAKE) --no-print-directory -C src/LH4051-FM clean
	@$(MAKE) --no-print-directory -C src/LH4051-SESSION clean

run: all
	@printf "$(GREEN)==> Running LH4051-DE $(VERSION)$(RESET)\n"
	@./$(TARGET)

help:
	@printf "$(CYAN)LH4051-DE $(VERSION)$(RESET)\n"
	@printf "  $(GREEN)make$(RESET)                 Build everything\n"
	@printf "  $(GREEN)make ARCH=cachy-v3$(RESET)   Build for x86-64-v3\n"
	@printf "  $(GREEN)make ARCH=cachy-v4$(RESET)   Build for x86-64-v4\n"
	@printf "  $(GREEN)make run$(RESET)             Build and run\n"
	@printf "  $(GREEN)make clean$(RESET)           Remove all build artifacts\n"
