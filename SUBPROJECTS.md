# LH4051-DE components

All components live in `src/` and are compiled into the single `lh4051-de` binary.

- **LH4051-WM** (`src/LH4051-WM`) — window registry and Longhorn window chrome. `lh4051_wm_create_window()` builds a framed
  window (title bar, min/max/close), registers it for the taskbar and unregisters it on destroy; tracks focus changes.
- **LH4051-FM** (`src/LH4051-FM`) — directory-listing backend (single enumerate call, size/type/date/icon, filter, sort).
- **LH4051-SESSION** (`src/LH4051-SESSION`) — chooses layer-shell or composite mode and assembles desktop, panel, sidebar and Start menu.
- `src/desktop.c`, `panel.c`, `sidebar.c`, `start_menu.c` — shell widgets.
- `src/explorer.c`, `settings.c`, `appstore.c` — applications.
- `src/platform.c` (OS differences), `prefs.c` (settings file), `theme.c` + `data/theme.css` (look, embedded as a GResource).
