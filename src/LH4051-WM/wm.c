#include "wm.h"

typedef struct { GtkWindow *window; char *title; } LHWindow;

static GPtrArray *windows;
static LH4051WMChangedFunc changed_cb;
static void *changed_data;
static GtkWindow *last_active; /* last focused app window (composite mode keeps the shell focused) */
static guint last_active_timeout;

/* Focus moves to the shell for a moment when a taskbar button is pressed, so the window that just lost focus
 * stays "last active" (for the click-to-minimize toggle) only for this long. Afterwards the taskbar follows
 * the real focus again, e.g. when switching with Alt+Tab or clicking the desktop. */
#define LAST_ACTIVE_GRACE_MS 600

static void emit_changed(void) { if (changed_cb) changed_cb(changed_data); }

static void cancel_last_active_timeout(void)
{
    if (last_active_timeout) { g_source_remove(last_active_timeout); last_active_timeout = 0; }
}

static gboolean last_active_expired(gpointer data)
{
    (void)data;
    last_active_timeout = 0;
    last_active = NULL;
    emit_changed();
    return G_SOURCE_REMOVE;
}
static void free_window(gpointer data) { LHWindow *w = data; g_free(w->title); g_free(w); }

static LHWindow *find(GtkWindow *window, guint *index)
{
    if (!windows) return NULL;
    for (guint i = 0; i < windows->len; i++) {
        LHWindow *x = g_ptr_array_index(windows, i);
        if (x->window == window) { if (index) *index = i; return x; }
    }
    return NULL;
}

void lh4051_wm_init(void)
{
    if (!windows) windows = g_ptr_array_new_with_free_func(free_window);
}

void lh4051_wm_shutdown(void)
{
    changed_cb = NULL;
    changed_data = NULL;
    last_active = NULL;
    cancel_last_active_timeout();
    g_clear_pointer(&windows, g_ptr_array_unref);
}

static void on_active_changed(GObject *obj, GParamSpec *pspec, gpointer data)
{
    (void)pspec; (void)data;
    if (gtk_window_is_active(GTK_WINDOW(obj))) {
        cancel_last_active_timeout();
        last_active = GTK_WINDOW(obj);
    } else if (GTK_WINDOW(obj) == last_active && !last_active_timeout) {
        last_active_timeout = g_timeout_add(LAST_ACTIVE_GRACE_MS, last_active_expired, NULL);
    }
    emit_changed();
}

static void on_destroyed(GtkWidget *w, gpointer data)
{
    (void)data;
    lh4051_wm_unregister(GTK_WINDOW(w));
}

void lh4051_wm_register(GtkWindow *window, const char *title)
{
    if (!window) return;
    lh4051_wm_init();
    if (find(window, NULL)) { lh4051_wm_set_title(window, title); return; }
    LHWindow *x = g_new0(LHWindow, 1);
    x->window = window;
    x->title = g_strdup(title ? title : "LH4051 Window");
    g_ptr_array_add(windows, x);
    g_signal_connect(window, "notify::is-active", G_CALLBACK(on_active_changed), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(on_destroyed), NULL);
    emit_changed();
}

void lh4051_wm_unregister(GtkWindow *window)
{
    guint i;
    if (!window || !find(window, &i)) return;
    g_signal_handlers_disconnect_by_func(window, G_CALLBACK(on_active_changed), NULL);
    g_signal_handlers_disconnect_by_func(window, G_CALLBACK(on_destroyed), NULL);
    if (last_active == window) { last_active = NULL; cancel_last_active_timeout(); }
    g_ptr_array_remove_index(windows, i);
    emit_changed();
}

void lh4051_wm_set_title(GtkWindow *window, const char *title)
{
    LHWindow *x = find(window, NULL);
    GtkLabel *label;
    if (!x) return;
    g_free(x->title);
    x->title = g_strdup(title ? title : "LH4051 Window");
    gtk_window_set_title(window, x->title);
    label = g_object_get_data(G_OBJECT(window), "lh-title-label");
    if (label) gtk_label_set_text(label, x->title);
    emit_changed();
}

guint lh4051_wm_count(void) { return windows ? windows->len : 0; }

GtkWindow *lh4051_wm_get_window(guint i)
{
    return (windows && i < windows->len) ? ((LHWindow *)g_ptr_array_index(windows, i))->window : NULL;
}

const char *lh4051_wm_get_title(guint i)
{
    return (windows && i < windows->len) ? ((LHWindow *)g_ptr_array_index(windows, i))->title : NULL;
}

gboolean lh4051_wm_is_active(guint i)
{
    GtkWindow *w = lh4051_wm_get_window(i);
    return w && (w == last_active || gtk_window_is_active(w));
}

void lh4051_wm_activate(GtkWindow *window) { if (window) gtk_window_present(window); }

void lh4051_wm_toggle(GtkWindow *window)
{
    if (!window) return;
    if (window == last_active) { last_active = NULL; cancel_last_active_timeout(); gtk_window_minimize(window); emit_changed(); }
    else gtk_window_present(window);
}

void lh4051_wm_set_changed_callback(LH4051WMChangedFunc callback, void *user_data)
{
    changed_cb = callback;
    changed_data = user_data;
}

/* ---- window chrome ---- */

static void win_minimize(GtkButton *b, gpointer d) { (void)b; gtk_window_minimize(GTK_WINDOW(d)); }
static void win_close(GtkButton *b, gpointer d) { (void)b; gtk_window_close(GTK_WINDOW(d)); }
static void win_maximize(GtkButton *b, gpointer d)
{
    GtkWindow *w = GTK_WINDOW(d);
    (void)b;
    if (gtk_window_is_maximized(w)) gtk_window_unmaximize(w);
    else gtk_window_maximize(w);
}

static GtkWidget *chrome_button(const char *icon, const char *css, GCallback cb, GtkWindow *win)
{
    GtkWidget *b = gtk_button_new_from_icon_name(icon);
    gtk_widget_add_css_class(b, "lh-winbtn");
    if (css) gtk_widget_add_css_class(b, css);
    gtk_widget_set_focusable(b, FALSE);
    g_signal_connect(b, "clicked", cb, win);
    return b;
}

GtkWindow *lh4051_wm_create_window(GtkApplication *app, const char *title,
                                   int width, int height, GtkWidget *content)
{
    GtkWidget *w = gtk_application_window_new(app);
    GtkWindow *win = GTK_WINDOW(w);
    GtkWidget *handle = gtk_window_handle_new();
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *label = gtk_label_new(title);

    gtk_window_set_title(win, title);
    gtk_window_set_default_size(win, width, height);

    gtk_widget_add_css_class(bar, "lh-titlebar");
    gtk_widget_add_css_class(label, "lh-title");
    gtk_widget_set_hexpand(label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(bar), label);
    gtk_box_append(GTK_BOX(bar), chrome_button("window-minimize-symbolic", NULL, G_CALLBACK(win_minimize), win));
    gtk_box_append(GTK_BOX(bar), chrome_button("window-maximize-symbolic", NULL, G_CALLBACK(win_maximize), win));
    gtk_box_append(GTK_BOX(bar), chrome_button("window-close-symbolic", "lh-winbtn-close", G_CALLBACK(win_close), win));
    gtk_window_handle_set_child(GTK_WINDOW_HANDLE(handle), bar);
    gtk_window_set_titlebar(win, handle);

    g_object_set_data(G_OBJECT(win), "lh-title-label", label);
    if (content) gtk_window_set_child(win, content);
    lh4051_wm_register(win, title);
    return win;
}
