# LH4051-DE
A Linux desktop environment replicating **Windows Longhorn Build 4051** using **GTK 4**.

## Requirements
- `gcc`
- `make`
- `pkg-config`
- `libgtk-4-dev`

## Building with CachyOS Optimization Targets
You can specify microarchitecture optimizations directly when compiling:

```bash
# Build using CachyOS v3 optimizations (default)
make ARCH=cachy-v3

# Build using CachyOS v4 optimizations (AVX-512)
make ARCH=cachy-v4

# Standard x86-64-v3 optimization
make ARCH=x86_64v3

# Install system-wide
sudo make install
