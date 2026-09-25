#include "wm.h"
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

typedef struct { GtkWindow *window; char *title; } LHWindow;
static GPtrArray *windows;
static LH4051WMChangedFunc changed_cb;
static void *changed_data;
static char *backend_name = NULL;

static void emit_changed(void){ if(changed_cb) changed_cb(changed_data); }
static void free_window(gpointer data){ LHWindow *w=data; if(!w) return; g_free(w->title); g_free(w); }
static void ensure_store(void){ if(!windows) windows=g_ptr_array_new_with_free_func(free_window); }

void lh4051_wm_init(void){ ensure_store(); }

void lh4051_wm_set_backend(GdkDisplay *display)
{
    g_free(backend_name);
    backend_name = g_strdup(display && GDK_IS_WAYLAND_DISPLAY(display) ? "wayland" :
                            (display && GDK_IS_X11_DISPLAY(display) ? "x11" : "gtk"));
}

void lh4051_wm_shutdown(void)
{
    if(windows){g_ptr_array_unref(windows);windows=NULL;}
    g_clear_pointer(&backend_name,g_free);
}

void lh4051_wm_register(GtkWindow *window,const char *title)
{
    if (!window)
        return;
    ensure_store();
    for(guint i=0;i<windows->len;i++){
        LHWindow *x=g_ptr_array_index(windows,i);
        if(x->window==window){lh4051_wm_set_title(window,title);return;}
    }
    LHWindow *x=g_new0(LHWindow,1);
    x->window=window;
    x->title=g_strdup(title?title:"LH4051 Window");
    g_ptr_array_add(windows,x);
    g_object_set_data(G_OBJECT(window),"lh4051-wm-registered",GINT_TO_POINTER(TRUE));
    emit_changed();
}

void lh4051_wm_unregister(GtkWindow *window)
{
    if(!windows||!window)return;
    for(guint i=0;i<windows->len;i++){
        LHWindow*x=g_ptr_array_index(windows,i);
        if(x->window==window){g_ptr_array_remove_index(windows,i);emit_changed();return;}
    }
}

void lh4051_wm_set_title(GtkWindow *window,const char *title)
{
    if(!windows||!window)return;
    for(guint i=0;i<windows->len;i++){
        LHWindow*x=g_ptr_array_index(windows,i);
        if(x->window==window){
            g_free(x->title);
            x->title=g_strdup(title?title:"LH4051 Window");
            gtk_window_set_title(window,x->title);
            emit_changed();
            return;
        }
    }
}

guint lh4051_wm_count(void){return windows?windows->len:0;}
GtkWindow *lh4051_wm_get_window(guint index){if(!windows||index>=windows->len)return NULL;return ((LHWindow*)g_ptr_array_index(windows,index))->window;}
const char *lh4051_wm_get_title(guint index){if(!windows||index>=windows->len)return NULL;return ((LHWindow*)g_ptr_array_index(windows,index))->title;}
void lh4051_wm_activate(GtkWindow *window){if(window)gtk_window_present(window);}
void lh4051_wm_set_changed_callback(LH4051WMChangedFunc callback,void*user_data){changed_cb=callback;changed_data=user_data;}
const char *lh4051_wm_backend_name(void){return backend_name?backend_name:"unknown";}
