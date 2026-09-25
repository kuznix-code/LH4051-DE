#ifndef LH4051_SESSION_H
#define LH4051_SESSION_H
#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>

typedef enum {
    LH4051_SHELL_DESKTOP,
    LH4051_SHELL_PANEL,
    LH4051_SHELL_SIDEBAR,
    LH4051_SHELL_POPUP
} LH4051ShellRole;

void lh4051_session_start(GtkApplication *app);
void lh4051_session_stop(void);
GtkApplication *lh4051_session_app(void);
gboolean lh4051_session_is_wayland(void);
gboolean lh4051_session_is_x11(void);
const char *lh4051_session_backend_name(void);
void lh4051_session_setup_shell_window(GtkWindow *window, LH4051ShellRole role);
