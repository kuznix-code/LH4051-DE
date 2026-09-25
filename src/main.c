#include "longhorn.h"

static void on_activate(GtkApplication *app, gpointer user_data) {
    g_print("Starting LH4051-DE (Version %s) on GTK4...\n", VERSION);

    // Main desktop container / wallpaper window placeholder
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(G_TYPE_CHECK_INSTANCE_CAST(window, gtk_window_get_type(), GtkWindow), "Windows Longhorn Build 4051");
    gtk_window_set_default_size(GTK_TYPE_CHECK_INSTANCE_CAST(window, gtk_window_get_type(), GtkWindow), 1024, 768);

    // Initialize Longhorn Shell Elements (Plex Taskbar & Sidebar)
    create_longhorn_panel(app);
    create_longhorn_sidebar(app);

    gtk_window_present(GTK_TYPE_CHECK_INSTANCE_CAST(window, gtk_window_get_type(), GtkWindow));
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new("org.kuznix.lh4051de", G_APPLICATION_DEFAULT_FLAGS);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
