# LH4051-DE

A GTK4 desktop shell that recreates the look of **Windows "Longhorn" Build 4051** (PDC 2003):
slate taskbar with a pill-shaped Start button, translucent sidebar (search, analog clock, notes),
two-column Start menu, Longhorn-framed windows, and an Explorer with the blue "Phode" header.

Runs on **Linux** (Wayland layer-shell *or* any X11/Wayland desktop) and **Windows x64**
(MinGW-w64 or MSVC). Version: `0.0.0` (pre-release).

## How it runs

| Mode | When | What happens |
|------|------|--------------|
| **layer-shell** | Wayland compositor with `wlr-layer-shell` + `gtk4-layer-shell` installed | desktop, taskbar, sidebar and Start menu are real shell surfaces; the taskbar reserves screen space |
| **composite** | everything else (Windows, X11, GNOME/KDE Wayland, macOS) | the whole shell is drawn in one full-screen window; app windows open on top |

`lh4051-de --windowed` runs composite mode in a normal 1280x800 window (good for trying it out).
`LH4051_MODE=composite` forces composite mode on Wayland.

## Build

### Linux
```sh
# Debian/Ubuntu
sudo apt install build-essential pkg-config libgtk-4-dev libglib2.0-dev-bin libgtk4-layer-shell-dev
# Fedora: gtk4-devel gtk4-layer-shell-devel   Arch: gtk4 gtk4-layer-shell
make -j$(nproc)
make run ARGS=--windowed
sudo make install            # PREFIX=/usr/local by default; DESTDIR supported
make dist-linux              # dist/lh4051-de-<ver>-linux-<arch>.tar.gz
```
`gtk4-layer-shell` is optional; without it the shell uses composite mode.

### Windows — MinGW-w64 (MSYS2)
In an **MSYS2 MINGW64** shell:
```sh
pacman -S make zip mingw-w64-x86_64-toolchain mingw-w64-x86_64-pkgconf \
          mingw-w64-x86_64-gtk4 mingw-w64-x86_64-adwaita-icon-theme
make win64                   # build/windows-mingw/lh4051-de.exe
make dist-win64              # dist/lh4051-de-<ver>-win64.zip with the GTK runtime bundled
```
Cross-compiling from Linux works too if a MinGW GTK4 sysroot exists (`MINGW_PREFIX=/usr/x86_64-w64-mingw32`,
or an MXE/Fedora `x86_64-w64-mingw32-pkg-config`): `make win64`.

### Windows — MSVC
From an **x64 Native Tools Command Prompt** with GNU make, pkgconf/pkg-config and a
[gvsbuild](https://github.com/wingtk/gvsbuild) GTK4 tree:
```bat
make msvc GTK_PREFIX=C:/gtk GLIB_COMPILE_RESOURCES=C:/gtk/bin/glib-compile-resources.exe
```
Make sure MSVC's `link.exe` comes before MSYS2's coreutils `link` in `PATH`.
At runtime put `C:\gtk\bin` next to the exe or on `PATH`.

### Useful variables
`DEBUG=1` · `LTO=1` · `MARCH=x86-64-v3` · `CONSOLE=1` (Windows console for logs) ·
`LAYER_SHELL=auto|1|0` · `CC=clang` · `BUILD=<dir>` · `make show-config` prints the resolved setup.
(The old `ARCH=`/`TARGET=` interface and the 200 hard-coded toolchain targets are gone; use `CC`, `MARCH`, `TRIPLE`.)

## Using it
* **Start** opens the menu: pinned places, *All Programs* (searchable), user block, Log Off / Turn Off (closes the shell).
* **Taskbar** buttons track open windows; click to focus or minimize.
* **Sidebar**: Search (opens Explorer filtered by your text), Clock, Notes (autosaved). Use the edge handle to collapse.
* **Explorer**: back/forward/up, path bar, live filter, sortable columns, hidden-file toggle, favorite links.
* **Settings**: desktop icons, watermark, wallpaper, 24-hour clock, sidebar. Stored in `~/.config/lh4051-de/settings.ini`
  (`%APPDATA%\lh4051-de` on Windows).
* **App Store**: installed apps; AppStream search + install/remove via PackageKit/pacman/apt/dnf/zypper (Linux) or `winget` (Windows).

On Windows the shell covers the screen. Press **Alt+F4** on the shell, or use *Start → Turn Off Computer*, to exit;
`--windowed` avoids this.

## Sessions (Linux login managers)
`make install` installs `lh4051-de-session` and Wayland/X11 session entries. The Wayland session starts
`labwc` (or `sway`) with the shell as its autostart; set `LH4051_COMPOSITOR` to use another layer-shell compositor.

The project does not bundle Microsoft artwork; the wallpaper is an included image and can be replaced in Settings.
