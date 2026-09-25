#include "longhorn.h"

static void load_shell_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        "window.lh-panel { background-color: rgba(35, 40, 45, 0.85); }"
        "window.lh-panel.taskbar { border-top: 1px solid rgba(255, 255, 255, 0.15); }"
        "window.lh-panel.sidebar { border-left: 1px solid rgba(255, 255, 255, 0.15); }"
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    g_print("Starting LH4051-DE Layer Shell (Version %s)...\n", VERSION);

    load_shell_css();
    create_longhorn_panel(app);
    create_longhorn_sidebar(app);
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new("org.kuznix.lh4051de", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
