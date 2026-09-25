# LH4051-DE components

All LH4051-DE components now live at repository root. The old `subprojects/` tree has been removed.

- **LH4051-WM** — complete in-process window manager/tracker with registration, unregister, title updates, activation, enumeration, and taskbar change notifications.
- **LH4051-FM** — reusable native directory-listing backend.
- **LH4051-SESSION** — desktop-session lifecycle: starts the desktop shell and owns WM initialization/shutdown.
- **LH4051-START** — Start menu implementation in `src/start_menu.c`.
- **LH4051-TASKBAR** — taskbar implementation in `src/panel.c`, now driven by LH4051-WM.
- **LH4051-SIDEBAR** — sidebar implementation in `src/sidebar.c`.

The build is a single integrated LH4051-DE binary with version `0.0.0`.
