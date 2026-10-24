#ifndef LH4051_WM_H
#define LH4051_WM_H
#include <gtk/gtk.h>

typedef void (*LH4051WMChangedFunc)(void *user_data);

void lh4051_wm_init(void);
void lh4051_wm_shutdown(void);

/* Creates a Longhorn-framed application window (custom title bar with
 * minimize/maximize/close) around `content`, registers it for the taskbar and
 * unregisters it automatically when destroyed. */
GtkWindow *lh4051_wm_create_window(GtkApplication *app, const char *title,
                                   int width, int height, GtkWidget *content);

void lh4051_wm_register(GtkWindow *window, const char *title);
void lh4051_wm_unregister(GtkWindow *window);
void lh4051_wm_set_title(GtkWindow *window, const char *title);
guint lh4051_wm_count(void);
GtkWindow *lh4051_wm_get_window(guint index);
const char *lh4051_wm_get_title(guint index);
gboolean lh4051_wm_is_active(guint index);
void lh4051_wm_activate(GtkWindow *window);
void lh4051_wm_toggle(GtkWindow *window); /* minimize if active, else present */
void lh4051_wm_set_changed_callback(LH4051WMChangedFunc callback, void *user_data);

#endif
