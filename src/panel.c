#include "longhorn.h"

void create_longhorn_panel(GtkApplication *app) {
    GtkWidget *panel = gtk_application_window_new(app);
    GtkWindow *window = GTK_WINDOW(panel);
    gtk_window_set_title(window, "Longhorn Taskbar");
    gtk_window_set_default_size(window, 1024, 40);
    // Taskbar layout controls (Start button, tray, application bar) go here
    gtk_window_present(window);
}
