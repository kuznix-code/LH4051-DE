# LH4051-DE Makefile
VERSION = 0.0.0
CC = gcc
PKG_CONFIG = pkg-config
ARCH ?= cachy-v3
BASE_CFLAGS = -Wall -Wextra -O3 -pipe -DVERSION=\"$(VERSION)\"
ifeq ($(ARCH),cachy-v3)
ARCH_CFLAGS = -march=x86-64-v3 -O3 -pipe -fno-plt -fexceptions -Wp,-D_FORTIFY_SOURCE=3 -Wformat -Werror=format-security -fstack-clash-protection -fcf-protection -Wp,-D_GLIBCXX_ASSERTIONS
ARCH_LDFLAGS = -Wl,-O1 -Wl,--sort-common -Wl,--as-needed -Wl,-z,relro -Wl,-z,now -Wl,-z,pack-relative-relocs -flto=auto
else ifeq ($(ARCH),cachy-v4)
ARCH_CFLAGS = -march=x86-64-v4 -O3 -pipe -fno-plt -fexceptions -Wp,-D_FORTIFY_SOURCE=3 -Wformat -Werror=format-security -fstack-clash-protection -fcf-protection -Wp,-D_GLIBCXX_ASSERTIONS
ARCH_LDFLAGS = -Wl,-O1 -Wl,--sort-common -Wl,--as-needed -Wl,-z,relro -Wl,-z,now -Wl,-z,pack-relative-relocs -flto=auto
else
ARCH_CFLAGS = -march=x86-64-v3
ARCH_LDFLAGS =
endif
GTK_CFLAGS = $(shell $(PKG_CONFIG) --cflags gtk4 gtk4-layer-shell-0)
GTK_LIBS = $(shell $(PKG_CONFIG) --libs gtk4 gtk4-layer-shell-0)
CFLAGS = $(BASE_CFLAGS) $(ARCH_CFLAGS) $(GTK_CFLAGS) -Isubprojects/LH4051-WM -Isubprojects/LH4051-FM -Isubprojects/LH4051-SHELL
LDFLAGS = $(ARCH_LDFLAGS) $(GTK_LIBS) -lm
SRC = src/main.c src/desktop.c src/panel.c src/sidebar.c src/start_menu.c src/explorer.c subprojects/LH4051-WM/wm.c subprojects/LH4051-FM/fm.c subprojects/LH4051-SHELL/shell.c
OBJDIR = build
OBJ = $(patsubst %.c,$(OBJDIR)/%.o,$(SRC))
TARGET = $(OBJDIR)/lh4051-de
.PHONY: all clean run
all: $(TARGET)
$(TARGET): $(OBJ)
	@mkdir -p $(OBJDIR)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)
$(OBJDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -rf $(OBJDIR)
run: all
	./$(TARGET)
