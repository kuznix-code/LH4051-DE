#include "session.h"
#include "../LH4051-WM/wm.h"
#include "../longhorn.h"
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

static GtkApplication *session_app;
static gboolean session_wayland;
static gboolean session_x11;

void lh4051_session_start(GtkApplication *app)
{
    GdkDisplay *display = gdk_display_get_default();
    session_app = app;
    session_wayland = display && GDK_IS_WAYLAND_DISPLAY(display);
    session_x11 = display && GDK_IS_X11_DISPLAY(display);

    g_print("LH4051 session backend: %s\n",
            session_wayland ? "Wayland" : (session_x11 ? "X11" : "GTK"));

    lh4051_wm_init();
    lh4051_wm_set_backend(display);

    create_desktop(app);
    create_longhorn_panel(app);
    create_longhorn_sidebar(app);
}

void lh4051_session_stop(void)
{
    lh4051_wm_shutdown();
    session_app = NULL;
    session_wayland = FALSE;
    session_x11 = FALSE;
}

GtkApplication *lh4051_session_app(void)
{
    return session_app;
}

gboolean lh4051_session_is_wayland(void)
{
    return session_wayland;
}

gboolean lh4051_session_is_x11(void)
{
    return session_x11;
}

const char *lh4051_session_backend_name(void)
{
    return session_wayland ? "wayland" : (session_x11 ? "x11" : "other");
}

void lh4051_session_setup_shell_window(GtkWindow *window, LH4051ShellRole role)
{
    if (!window)
        return;

    if (session_wayland) {
        gtk_layer_init_for_window(window);
        gtk_layer_set_namespace(window, "lh4051-de");

        switch (role) {
        case LH4051_SHELL_DESKTOP:
            gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_BACKGROUND);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
            break;
        case LH4051_SHELL_PANEL:
            gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_TOP);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
            gtk_layer_auto_exclusive_zone_enable(window);
            break;
        case LH4051_SHELL_SIDEBAR:
            gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_TOP);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
            break;
        case LH4051_SHELL_POPUP:
            gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
            break;
        }
        return;
    }

    /* X11 and other GDK backends use ordinary GTK windows. */
    gtk_window_set_decorated(window, role != LH4051_SHELL_PANEL);
    if (role == LH4051_SHELL_DESKTOP)
        gtk_window_fullscreen(window);
}
