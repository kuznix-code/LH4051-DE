#include "longhorn.h"

void create_longhorn_sidebar(GtkApplication *app) {
    GtkWidget *window = gtk_application_window_new(app);
    GtkWindow *gtk_win = GTK_WINDOW(window);
    
    gtk_layer_init_for_window(gtk_win);
    gtk_layer_set_layer(gtk_win, GTK_LAYER_SHELL_LAYER_TOP);
    
    // Anchor to right, top, and bottom
    gtk_layer_set_anchor(gtk_win, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_anchor(gtk_win, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(gtk_win, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    
    // Set margin bottom so it doesn't overlap the taskbar (40px)
    gtk_layer_set_margin(gtk_win, GTK_LAYER_SHELL_EDGE_BOTTOM, 40);
    
    // Reserve screen space
    gtk_layer_auto_exclusive_zone_enable(gtk_win);
    
    gtk_widget_set_size_request(window, 220, -1); // 220px wide
    
    // Apply styling
    gtk_widget_add_css_class(window, "lh-panel");
    gtk_widget_add_css_class(window, "sidebar");
    
    gtk_window_present(gtk_win);
}
