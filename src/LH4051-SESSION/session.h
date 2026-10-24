#ifndef LH4051_SESSION_H
#define LH4051_SESSION_H
#include <gtk/gtk.h>

/* Layer mode: Wayland + gtk4-layer-shell, one surface per shell component.
 * Composite mode (Windows, X11, macOS, Wayland without layer-shell): the whole
 * shell is drawn inside a single full-screen (or --windowed) window. */
void lh4051_session_start(GtkApplication *app, gboolean windowed);
void lh4051_session_stop(void);
GtkApplication *lh4051_session_app(void);
gboolean lh4051_session_uses_layer_shell(void);
const char *lh4051_session_backend_name(void);
void lh4051_session_toggle_start_menu(void);
void lh4051_session_hide_start_menu(void);
void lh4051_session_quit(void);

#endif
