# LH4051-DE

GTK4 + gtk4-layer-shell Wayland desktop environment inspired by the visual language of Windows Longhorn PDC 2003 Build 4051.

Version: 0.0.0. There are no public releases yet, so the development version intentionally remains 0.0.0.

## Implemented shell

- Full-screen background layer and Longhorn-style gradient desktop
- Recycle Bin desktop shortcut
- Build/version watermark
- Slate gradient bottom taskbar
- Start button and interactive Start menu
- My Documents task button
- System tray, volume/network indicators, hidden-icon affordance and live clock
- Right-side translucent sidebar
- Analog clock with live hands
- Digital time/date
- Sidebar widget area and controls
- Two-column Start menu
- Administrator identity block
- Application shortcuts and All Programs affordance
- Documents, Pictures, Videos and system links
- Log Off and Turn Off Computer controls
- GTK4 Explorer prototype
- Longhorn-style title bar and close button
- File/Edit/View/Favorites/Tools/Help menu row
- Back/forward/up toolbar
- Directory path and search fields
- Blue Phode-style metadata header
- Filter sidebar
- Name/Size/Type/Date Modified columns
- Live directory listing

The project does not bundle Microsoft's original wallpaper artwork. The desktop currently uses a built-in gradient placeholder and can be extended with an asset/resource system later.

## Build

make clean
make ARCH=cachy-v3 -j4 -l4
make run

Requirements: GTK4, gtk4-layer-shell, Wayland layer-shell compositor, GCC and pkg-config.

This is an experimental pre-release development tree. Layout, APIs and implementation details can change before the first public release.
