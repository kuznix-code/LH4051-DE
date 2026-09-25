#ifndef LH4051_WM_H
#define LH4051_WM_H
#include <gtk/gtk.h>
typedef void (*LH4051WMChangedFunc)(void *user_data);
void lh4051_wm_init(void);
void lh4051_wm_shutdown(void);
void lh4051_wm_register(GtkWindow *window, const char *title);
void lh4051_wm_unregister(GtkWindow *window);
void lh4051_wm_set_title(GtkWindow *window, const char *title);
guint lh4051_wm_count(void);
GtkWindow *lh4051_wm_get_window(guint index);
const char *lh4051_wm_get_title(guint index);
void lh4051_wm_activate(GtkWindow *window);
void lh4051_wm_set_changed_callback(LH4051WMChangedFunc callback, void *user_data);
#endif
