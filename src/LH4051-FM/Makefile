# LH4051-FM subproject
CC ?= gcc
PKG_CONFIG ?= pkg-config
ARCH ?= cachy-v3
VERSION ?= 0.0.0
BUILD_DIR := build
TARGET := $(BUILD_DIR)/fm.o
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk4)
ARCH_CFLAGS := $(if $(filter cachy-v4,$(ARCH)),-march=x86-64-v4,-march=x86-64-v3)
CFLAGS ?= -Wall -Wextra -O3 -pipe -DVERSION=\\"$(VERSION)\\" $(ARCH_CFLAGS) $(GTK_CFLAGS)

.PHONY: all clean
all: $(TARGET)

$(TARGET): fm.c fm.h
	@mkdir -p $(BUILD_DIR)
	@printf "\033[1;34mCC  %s\033[0m\n" "$<"
	@$(CC) $(CFLAGS) -c fm.c -o $@

clean:
	@rm -rf $(BUILD_DIR)
