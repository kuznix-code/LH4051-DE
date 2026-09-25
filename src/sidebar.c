#include "longhorn.h"

void create_longhorn_sidebar(GtkApplication *app) {
    GtkWidget *sidebar = gtk_application_window_new(app);
    GtkWindow *window = GTK_WINDOW(sidebar);
    gtk_window_set_title(window, "Plex Sidebar");
    gtk_window_set_default_size(window, 200, 768);
    // Plex style sidebar panel (analog clock tile, search, notes) go here
    gtk_window_present(window);
}
