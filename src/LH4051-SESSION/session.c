#include "session.h"
#include "../LH4051-WM/wm.h"
#include "../longhorn.h"

#ifdef LH_HAVE_LAYER_SHELL
#include <gtk4-layer-shell.h>
#endif

static GtkApplication *session_app;
static const char *backend = "unknown";
static gboolean layer_mode;

static GtkWidget *start_widget;   /* the start menu contents */
static GtkWindow *start_window;   /* layer mode only */
static gint64 start_hidden_at;    /* guards "click Start to close" races */
static gboolean start_was_active;

static void sidebar_visibility(gpointer data)
{
    gtk_widget_set_visible(GTK_WIDGET(data), lh4051_prefs_get(LH_PREF_SIDEBAR));
}

/* ---------- start menu visibility ---------- */

static gboolean start_visible(void)
{
    if (layer_mode) return start_window && gtk_widget_get_visible(GTK_WIDGET(start_window));
    return start_widget && gtk_widget_get_visible(start_widget);
}

static void start_set_visible(gboolean show)
{
    if (!start_widget) return;
    if (show) {
        lh4051_start_menu_reset();
        start_was_active = FALSE;
        if (layer_mode) gtk_window_present(start_window);
        else gtk_widget_set_visible(start_widget, TRUE);
    } else {
        start_hidden_at = g_get_monotonic_time();
        if (layer_mode) gtk_widget_set_visible(GTK_WIDGET(start_window), FALSE);
        else gtk_widget_set_visible(start_widget, FALSE);
    }
}

void lh4051_session_hide_start_menu(void) { if (start_visible()) start_set_visible(FALSE); }

void lh4051_session_toggle_start_menu(void)
{
    if (start_visible()) { start_set_visible(FALSE); return; }
    /* In layer mode the menu closes itself when focus moves to the panel, so a
     * click on Start that just closed it must not reopen it. */
    if (g_get_monotonic_time() - start_hidden_at < 250 * 1000) return;
    start_set_visible(TRUE);
}

void lh4051_session_quit(void)
{
    if (session_app) g_application_quit(G_APPLICATION(session_app));
}

static void desktop_pressed(GtkGestureClick *g, int n, double x, double y, gpointer d)
{
    (void)g; (void)n; (void)x; (void)y; (void)d;
    lh4051_session_hide_start_menu();
}

/* ---------- layer-shell mode ---------- */

#ifdef LH_HAVE_LAYER_SHELL
static GtkWindow *layer_window(GtkApplication *app, GtkWidget *child, const char *ns,
                               GtkLayerShellLayer layer, gboolean top, gboolean bottom,
                               gboolean left, gboolean right)
{
    GtkWindow *win = GTK_WINDOW(gtk_application_window_new(app));
    gtk_window_set_decorated(win, FALSE);
    gtk_layer_init_for_window(win);
    gtk_layer_set_namespace(win, ns);
    gtk_layer_set_layer(win, layer);
    gtk_layer_set_anchor(win, GTK_LAYER_SHELL_EDGE_TOP, top);
    gtk_layer_set_anchor(win, GTK_LAYER_SHELL_EDGE_BOTTOM, bottom);
    gtk_layer_set_anchor(win, GTK_LAYER_SHELL_EDGE_LEFT, left);
    gtk_layer_set_anchor(win, GTK_LAYER_SHELL_EDGE_RIGHT, right);
    gtk_window_set_child(win, child);
    return win;
}

static void start_active_changed(GObject *obj, GParamSpec *p, gpointer d)
{
    (void)p; (void)d;
    if (gtk_window_is_active(GTK_WINDOW(obj))) start_was_active = TRUE;
    else if (start_was_active && start_visible()) start_set_visible(FALSE);
}

static void start_layer_mode(GtkApplication *app)
{
    GtkWindow *desktop, *panel, *sidebar;
    GtkWidget *desktop_widget = lh4051_desktop_new(app);
    GtkGesture *dclick = gtk_gesture_click_new();

    g_signal_connect(dclick, "pressed", G_CALLBACK(desktop_pressed), NULL);
    gtk_widget_add_controller(desktop_widget, GTK_EVENT_CONTROLLER(dclick));
    desktop = layer_window(app, desktop_widget, "lh4051-desktop",
                           GTK_LAYER_SHELL_LAYER_BACKGROUND, TRUE, TRUE, TRUE, TRUE);
    gtk_layer_set_exclusive_zone(desktop, -1);

    panel = layer_window(app, lh4051_panel_new(app), "lh4051-panel",
                         GTK_LAYER_SHELL_LAYER_TOP, FALSE, TRUE, TRUE, TRUE);
    gtk_widget_set_size_request(GTK_WIDGET(panel), 1, LH4051_PANEL_HEIGHT);
    gtk_layer_auto_exclusive_zone_enable(panel);

    sidebar = layer_window(app, lh4051_sidebar_new(app), "lh4051-sidebar",
                           GTK_LAYER_SHELL_LAYER_TOP, TRUE, TRUE, FALSE, TRUE);
    gtk_layer_set_margin(sidebar, GTK_LAYER_SHELL_EDGE_BOTTOM, LH4051_PANEL_HEIGHT);
    gtk_layer_auto_exclusive_zone_enable(sidebar);

    start_widget = lh4051_start_menu_new(app);
    start_window = layer_window(app, start_widget, "lh4051-start",
                                GTK_LAYER_SHELL_LAYER_OVERLAY, FALSE, TRUE, TRUE, FALSE);
    gtk_layer_set_margin(start_window, GTK_LAYER_SHELL_EDGE_BOTTOM, LH4051_PANEL_HEIGHT);
    gtk_layer_set_keyboard_mode(start_window, GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
    g_signal_connect(start_window, "notify::is-active", G_CALLBACK(start_active_changed), NULL);

    gtk_window_present(desktop);
    gtk_window_present(panel);
    gtk_widget_set_visible(GTK_WIDGET(sidebar), lh4051_prefs_get(LH_PREF_SIDEBAR));
    lh4051_prefs_watch(sidebar_visibility, sidebar);
    if (lh4051_prefs_get(LH_PREF_SIDEBAR)) gtk_window_present(sidebar);
}
#endif

/* ---------- composite mode ---------- */

static gboolean shell_key(GtkEventControllerKey *c, guint keyval, guint code,
                          GdkModifierType state, gpointer d)
{
    (void)c; (void)code; (void)state; (void)d;
    if (keyval == GDK_KEY_Escape && start_visible()) { lh4051_session_hide_start_menu(); return TRUE; }
    return FALSE;
}

static void start_composite_mode(GtkApplication *app, gboolean windowed)
{
    GtkWidget *w = gtk_application_window_new(app);
    GtkWindow *win = GTK_WINDOW(w);
    GtkWidget *root = gtk_overlay_new();
    GtkWidget *desktop = lh4051_desktop_new(app);
    GtkWidget *panel = lh4051_panel_new(app);
    GtkWidget *sidebar = lh4051_sidebar_new(app);
    GtkGesture *click = gtk_gesture_click_new();
    GtkEventController *keys = gtk_event_controller_key_new();

    gtk_window_set_title(win, "LH4051-DE");
    gtk_overlay_set_child(GTK_OVERLAY(root), desktop);

    gtk_widget_set_halign(sidebar, GTK_ALIGN_END);
    gtk_widget_set_valign(sidebar, GTK_ALIGN_FILL);
    gtk_widget_set_margin_bottom(sidebar, LH4051_PANEL_HEIGHT);
    gtk_overlay_add_overlay(GTK_OVERLAY(root), sidebar);
    gtk_widget_set_visible(sidebar, lh4051_prefs_get(LH_PREF_SIDEBAR));
    lh4051_prefs_watch(sidebar_visibility, sidebar);

    start_widget = lh4051_start_menu_new(app);
    gtk_widget_set_halign(start_widget, GTK_ALIGN_START);
    gtk_widget_set_valign(start_widget, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(start_widget, LH4051_PANEL_HEIGHT);
    gtk_widget_set_visible(start_widget, FALSE);
    gtk_overlay_add_overlay(GTK_OVERLAY(root), start_widget);

    gtk_widget_set_valign(panel, GTK_ALIGN_END);
    gtk_widget_set_size_request(panel, -1, LH4051_PANEL_HEIGHT);
    gtk_overlay_add_overlay(GTK_OVERLAY(root), panel);

    g_signal_connect(click, "pressed", G_CALLBACK(desktop_pressed), NULL);
    gtk_widget_add_controller(desktop, GTK_EVENT_CONTROLLER(click));
    g_signal_connect(keys, "key-pressed", G_CALLBACK(shell_key), NULL);
    gtk_widget_add_controller(w, keys);

    gtk_window_set_child(win, root);
    if (windowed) {
        gtk_window_set_default_size(win, 1280, 800);
    } else {
        gtk_window_set_decorated(win, FALSE);
        gtk_window_fullscreen(win);
    }
    gtk_window_present(win);
}

/* ---------- entry points ---------- */

void lh4051_session_start(GtkApplication *app, gboolean windowed)
{
    GdkDisplay *display = gdk_display_get_default();
    const char *mode = g_getenv("LH4051_MODE");
    session_app = app;
    backend = lh4051_platform_backend(display);
    layer_mode = FALSE;

#ifdef LH_HAVE_LAYER_SHELL
    layer_mode = !windowed && gtk_layer_is_supported() &&
                 g_strcmp0(mode, "composite") != 0 && g_strcmp0(mode, "windowed") != 0;
#else
    (void)mode;
#endif

    g_print("LH4051-DE %s: backend=%s mode=%s\n", LH4051_VERSION, backend,
            layer_mode ? "layer-shell" : (windowed ? "windowed" : "composite"));

    lh4051_wm_init();
#ifdef LH_HAVE_LAYER_SHELL
    if (layer_mode) { start_layer_mode(app); return; }
#endif
    start_composite_mode(app, windowed);
}

void lh4051_session_stop(void)
{
    lh4051_wm_shutdown();
    session_app = NULL;
    start_widget = NULL;
    start_window = NULL;
}

GtkApplication *lh4051_session_app(void) { return session_app; }
gboolean lh4051_session_uses_layer_shell(void) { return layer_mode; }
const char *lh4051_session_backend_name(void) { return backend; }
