# LH4051-WM subproject
CC ?= gcc
PKG_CONFIG ?= pkg-config
ARCH ?= cachy-v3
VERSION ?= 0.0.0
BUILD_DIR := build
TARGET := $(BUILD_DIR)/wm.o
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk4)
ARCH_CFLAGS := $(if $(filter cachy-v4,$(ARCH)),-march=x86-64-v4,-march=x86-64-v3)
CFLAGS ?= -Wall -Wextra -O3 -pipe -DVERSION=\\"$(VERSION)\\" $(ARCH_CFLAGS) $(GTK_CFLAGS)

.PHONY: all clean
all: $(TARGET)

$(TARGET): wm.c wm.h
	@mkdir -p $(BUILD_DIR)
	@printf "\033[1;36mCC  %s\033[0m\n" "$<"
	@$(CC) $(CFLAGS) -c wm.c -o $@

clean:
	@rm -rf $(BUILD_DIR)
