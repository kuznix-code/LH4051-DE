# LH4051-DE Makefile (GTK4, Version 0.0.0)
VERSION = 0.0.0
CC = gcc
PKG_CONFIG = pkg-config

# Select Architecture Target: x86_64v2, x86_64v3, x86_64v4, cachy-v3, cachy-v4
ARCH ?= cachy-v3

# Base compilation flags
BASE_CFLAGS = -Wall -Wextra -O3 -pipe -DVERSION=\"$(VERSION)\"
BASE_LDFLAGS = 

ifeq ($(ARCH),x86_64v2)
    ARCH_CFLAGS = -march=x86-64-v2
    ARCH_LDFLAGS =
else ifeq ($(ARCH),x86_64v3)
    ARCH_CFLAGS = -march=x86-64-v3
    ARCH_LDFLAGS =
else ifeq ($(ARCH),x86_64v4)
    ARCH_CFLAGS = -march=x86-64-v4
    ARCH_LDFLAGS =
else ifeq ($(ARCH),cachy-v3)
    ARCH_CFLAGS = -march=x86-64-v3 -O3 -pipe -fno-plt -fexceptions \
                  -Wp,-D_FORTIFY_SOURCE=3 -Wformat -Werror=format-security \
                  -fstack-clash-protection -fcf-protection -Wp,-D_GLIBCXX_ASSERTIONS
    ARCH_LDFLAGS = -Wl,-O1 -Wl,--sort-common -Wl,--as-needed -Wl,-z,relro -Wl,-z,now \
                   -Wl,-z,pack-relative-relocs -flto=auto
else ifeq ($(ARCH),cachy-v4)
    ARCH_CFLAGS = -march=x86-64-v4 -O3 -pipe -fno-plt -fexceptions \
                  -Wp,-D_FORTIFY_SOURCE=3 -Wformat -Werror=format-security \
                  -fstack-clash-protection -fcf-protection -Wp,-D_GLIBCXX_ASSERTIONS
    ARCH_LDFLAGS = -Wl,-O1 -Wl,--sort-common -Wl,--as-needed -Wl,-z,relro -Wl,-z,now \
                   -Wl,-z,pack-relative-relocs -flto=auto
else
    ARCH_CFLAGS = -march=x86-64
    ARCH_LDFLAGS =
endif

GTK_CFLAGS = $(shell $(PKG_CONFIG) --cflags gtk4)
GTK_LIBS = $(shell $(PKG_CONFIG) --libs gtk4)

CFLAGS = $(BASE_CFLAGS) $(ARCH_CFLAGS) $(GTK_CFLAGS)
LDFLAGS = $(BASE_LDFLAGS) $(ARCH_LDFLAGS) $(GTK_LIBS)

SRC = src/main.c src/panel.c src/sidebar.c
OBJ = $(SRC:.c=.o)
TARGET = lh4051-de

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share/lh4051-de

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(DATADIR)/assets
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/
	install -m 644 assets/* $(DESTDIR)$(DATADIR)/assets/

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -rf $(DESTDIR)$(DATADIR)
