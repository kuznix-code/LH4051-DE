#include "longhorn.h"

void create_longhorn_panel(GtkApplication *app) {
    GtkWidget *window = gtk_application_window_new(app);
    GtkWindow *gtk_win = GTK_WINDOW(window);

    gtk_layer_init_for_window(gtk_win);
    gtk_layer_set_layer(gtk_win, GTK_LAYER_SHELL_LAYER_TOP);

    // Anchor to bottom, left, and right.
    gtk_layer_set_anchor(gtk_win, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(gtk_win, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(gtk_win, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

    // Give GTK a non-zero initial size; the left/right anchors determine width.
    gtk_widget_set_size_request(window, 1, 40);
    gtk_layer_set_size(gtk_win, 1, 40);

    // Reserve screen space.
    gtk_layer_auto_exclusive_zone_enable(gtk_win);

    // Apply styling.
    gtk_widget_add_css_class(window, "lh-panel");
    gtk_widget_add_css_class(window, "taskbar");

    gtk_window_present(gtk_win);
}
