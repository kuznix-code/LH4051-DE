#include "longhorn.h"

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    g_print("Starting LH4051-DE (Version %s) on GTK4...\n", VERSION);

    // Main desktop container / wallpaper window placeholder
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Windows Longhorn Build 4051");
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);

    // Initialize Longhorn Shell Elements (Plex Taskbar & Sidebar)
    create_longhorn_panel(app);
    create_longhorn_sidebar(app);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new("org.kuznix.lh4051de", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
