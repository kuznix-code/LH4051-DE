#ifndef LH4051_SESSION_H
#define LH4051_SESSION_H
#include <gtk/gtk.h>
void lh4051_session_start(GtkApplication *app);
void lh4051_session_stop(void);
GtkApplication *lh4051_session_app(void);
#endif
