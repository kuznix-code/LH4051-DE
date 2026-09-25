#include "longhorn.h"
#include "LH4051-SESSION/session.h"

static void on_explorer_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    create_file_explorer(GTK_APPLICATION(user_data), g_get_home_dir());
}

static GtkWidget *create_wallpaper(void)
{
    const char *path = g_getenv("LH4051_WALLPAPER");
    if (!path || !*path)
        path = "images/wallpapers.jpg";

    GtkWidget *picture = gtk_picture_new();
    GError *error = NULL;
    GdkTexture *texture = gdk_texture_new_from_filename(path, &error);

    if (texture) {
        gtk_picture_set_paintable(GTK_PICTURE(picture), GDK_PAINTABLE(texture));
        gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_COVER);
        gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
        gtk_widget_set_hexpand(picture, TRUE);
        gtk_widget_set_vexpand(picture, TRUE);
        g_object_unref(texture);
    } else {
        g_warning("LH4051 wallpaper: cannot load %s: %s",
                  path, error ? error->message : "unknown error");
        g_clear_error(&error);
        gtk_widget_add_css_class(picture, "lh-wallpaper-fallback");
    }

    return picture;
}

void create_desktop(GtkApplication *app)
{
    GtkWidget *w = gtk_application_window_new(app);
    GtkWindow *win = GTK_WINDOW(w);
    GtkWidget *root = gtk_overlay_new();
    GtkWidget *wallpaper = create_wallpaper();
    GtkWidget *desktop_layer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *top;
    GtkWidget *icons;
    GtkWidget *recycle;
    GtkWidget *watermark;

    lh4051_session_setup_shell_window(win, LH4051_SHELL_DESKTOP);
    gtk_widget_add_css_class(root, "lh-desktop");
    gtk_window_set_child(win, root);

    gtk_overlay_set_child(GTK_OVERLAY(root), wallpaper);

    gtk_widget_set_hexpand(desktop_layer, TRUE);
    gtk_widget_set_vexpand(desktop_layer, TRUE);
    gtk_overlay_add_overlay(GTK_OVERLAY(root), desktop_layer);

    top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(top, TRUE);
    gtk_box_append(GTK_BOX(desktop_layer), top);

    icons = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(icons, GTK_ALIGN_START);
    gtk_widget_set_valign(icons, GTK_ALIGN_START);
    gtk_widget_set_margin_top(icons, 18);
    gtk_widget_set_margin_start(icons, 14);
    gtk_box_append(GTK_BOX(top), icons);

    recycle = gtk_button_new_with_label("♻\nRecycle Bin");
    gtk_widget_set_size_request(recycle, 100, 78);
    gtk_box_append(GTK_BOX(icons), recycle);
    g_signal_connect(recycle, "clicked", G_CALLBACK(on_explorer_clicked), app);

    watermark = gtk_label_new("LH4051-DE\nWindows Longhorn PDC 2003 · Build 4051");
    gtk_widget_add_css_class(watermark, "lh-watermark");
    gtk_widget_set_halign(watermark, GTK_ALIGN_END);
    gtk_widget_set_valign(watermark, GTK_ALIGN_END);
    gtk_widget_set_margin_end(watermark, 12);
    gtk_widget_set_margin_bottom(watermark, 48);
    gtk_overlay_add_overlay(GTK_OVERLAY(root), watermark);

    gtk_window_present(win);
}
