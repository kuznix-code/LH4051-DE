#include "wm.h"
typedef struct { GtkWindow *window; char *title; } LHWindow;
static GPtrArray *windows;
static void ensure_store(void){ if(!windows) windows=g_ptr_array_new_with_free_func(g_free); }
void lh4051_wm_register(GtkWindow*w,const char*t){ ensure_store(); LHWindow*x=g_new0(LHWindow,1); x->window=w; x->title=g_strdup(t?t:"LH4051 Window"); g_ptr_array_add(windows,x); }
void lh4051_wm_unregister(GtkWindow*w){ if(!windows)return; for(guint i=0;i<windows->len;i++){LHWindow*x=g_ptr_array_index(windows,i);if(x->window==w){g_ptr_array_remove_index(windows,i);return;}}}
guint lh4051_wm_count(void){return windows?windows->len:0;}
