#ifndef LH4051_WM_H
#define LH4051_WM_H
#include <gtk/gtk.h>
void lh4051_wm_register(GtkWindow *window, const char *title);
void lh4051_wm_unregister(GtkWindow *window);
guint lh4051_wm_count(void);
#endif
