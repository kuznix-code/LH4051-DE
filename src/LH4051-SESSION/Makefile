# LH4051-SESSION subproject
CC ?= gcc
PKG_CONFIG ?= pkg-config
ARCH ?= cachy-v3
VERSION ?= 0.0.0
BUILD_DIR := build
TARGET := $(BUILD_DIR)/session.o
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk4 gtk4-layer-shell-0)
ARCH_CFLAGS := $(if $(filter cachy-v4,$(ARCH)),-march=x86-64-v4,-march=x86-64-v3)
CFLAGS ?= -Wall -Wextra -O3 -pipe -DVERSION=\\"$(VERSION)\\" $(ARCH_CFLAGS) $(GTK_CFLAGS) -I..

.PHONY: all clean
all: $(TARGET)

$(TARGET): session.c session.h ../LH4051-WM/wm.h ../longhorn.h
	@mkdir -p $(BUILD_DIR)
	@printf "\033[1;35mCC  %s\033[0m\n" "$<"
	@$(CC) $(CFLAGS) -c session.c -o $@

clean:
	@rm -rf $(BUILD_DIR)
