# LH4051-DE subprojects

LH4051-DE is the umbrella desktop project. Major components live under subprojects/ so they can evolve independently while remaining part of one integrated desktop.

- LH4051-WM — shell window-management/tracking API
- LH4051-FM — file-management backend
- LH4051-SHELL — desktop-shell lifecycle/integration
- LH4051-START — Start menu
- LH4051-TASKBAR — taskbar/tray
- LH4051-SIDEBAR — sidebar/widgets

The umbrella build links the reusable WM and FM components directly into LH4051-DE.
