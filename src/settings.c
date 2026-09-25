#include "longhorn.h"
#include "LH4051-SESSION/session.h"

static void close_settings(GtkButton *b, gpointer d)
{
    (void)b;
    gtk_window_destroy(GTK_WINDOW(d));
}

static void dark_mode_changed(GtkCheckButton *button, gpointer data)
{
    (void)data;
    GtkSettings *settings = gtk_settings_get_default();
    gboolean dark = gtk_check_button_get_active(button);
    if (settings)
        g_object_set(settings, "gtk-application-prefer-dark-theme", dark, NULL);
}

static GtkWidget *settings_card(const char *title, const char *description)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(card, "lh-settings-card");

    GtkWidget *heading = gtk_label_new(title);
    gtk_widget_add_css_class(heading, "lh-settings-heading");
    gtk_label_set_xalign(GTK_LABEL(heading), 0);
    gtk_box_append(GTK_BOX(card), heading);

    if (description) {
        GtkWidget *text = gtk_label_new(description);
        gtk_label_set_xalign(GTK_LABEL(text), 0);
        gtk_label_set_wrap(GTK_LABEL(text), TRUE);
        gtk_widget_add_css_class(text, "lh-settings-description");
        gtk_box_append(GTK_BOX(card), text);
    }

    return card;
}

void create_lh4051_settings(GtkApplication *app)
{
    GtkWidget *w = gtk_application_window_new(app);
    GtkWidget *root;
    GtkWidget *scroll;
    GtkWidget *box;
    GtkWindow *win = GTK_WINDOW(w);

    gtk_window_set_title(win, "LH4051 Settings");
    gtk_window_set_default_size(win, 760, 600);

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(root, "lh-settings");
    gtk_window_set_child(win, root);

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(bar, "lh-titlebar");
    GtkWidget *title = gtk_label_new("LH4051 Settings");
    gtk_widget_set_hexpand(title, TRUE);
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_box_append(GTK_BOX(bar), title);

    GtkWidget *close = gtk_button_new_with_label("✕");
    gtk_widget_add_css_class(close, "lh-close");
    g_signal_connect(close, "clicked", G_CALLBACK(close_settings), w);
    gtk_box_append(GTK_BOX(bar), close);
    gtk_box_append(GTK_BOX(root), bar);

    scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);
    gtk_box_append(GTK_BOX(root), scroll);

    GtkWidget *appearance = settings_card("Appearance",
        "Control the visual style used by LH4051-DE.");
    GtkWidget *dark = gtk_check_button_new_with_label("Use dark mode");
    GtkSettings *gtk_settings = gtk_settings_get_default();
    gboolean prefer_dark = FALSE;
    if (gtk_settings)
        g_object_get(gtk_settings, "gtk-application-prefer-dark-theme", &prefer_dark, NULL);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(dark), prefer_dark);
    g_signal_connect(dark, "toggled", G_CALLBACK(dark_mode_changed), NULL);
    gtk_box_append(GTK_BOX(appearance), dark);
    gtk_box_append(GTK_BOX(box), appearance);

    GtkWidget *desktop = settings_card("Desktop",
        "Desktop wallpaper, icons, and shell presentation.");
    gtk_box_append(GTK_BOX(desktop), gtk_check_button_new_with_label("Show desktop icons"));
    gtk_box_append(GTK_BOX(desktop), gtk_check_button_new_with_label("Show LH4051 build watermark"));
    gtk_box_append(GTK_BOX(box), desktop);

    GtkWidget *shell = settings_card("Taskbar & Start",
        "Taskbar, Start menu, task list, and system tray behavior.");
    gtk_box_append(GTK_BOX(shell), gtk_check_button_new_with_label("Show taskbar"));
    gtk_box_append(GTK_BOX(shell), gtk_check_button_new_with_label("Show Start menu search"));
    gtk_box_append(GTK_BOX(box), shell);

    GtkWidget *files = settings_card("File Manager",
        "Explorer presentation and file activation.");
    gtk_box_append(GTK_BOX(files), gtk_check_button_new_with_label("Open folders on activation"));
    gtk_box_append(GTK_BOX(files), gtk_check_button_new_with_label("Use default applications for files"));
    gtk_box_append(GTK_BOX(box), files);

    GtkWidget *apps = settings_card("Applications",
        "Application discovery uses the platform's registered application metadata.");
    gtk_box_append(GTK_BOX(apps), gtk_check_button_new_with_label("Show installed applications in Start"));
    gtk_box_append(GTK_BOX(apps), gtk_check_button_new_with_label("Use AppStream metadata when available"));
    gtk_box_append(GTK_BOX(box), apps);

    GtkWidget *system = settings_card("System",
        "Current session backend and shell architecture.");
    char *backend = g_strdup_printf("Session backend: %s", lh4051_session_backend_name());
    GtkWidget *backend_label = gtk_label_new(backend);
    g_free(backend);
    gtk_label_set_xalign(GTK_LABEL(backend_label), 0);
    gtk_box_append(GTK_BOX(system), backend_label);
    gtk_box_append(GTK_BOX(system), gtk_label_new("Window tracking: LH4051-WM"));
    gtk_box_append(GTK_BOX(box), system);

    gtk_window_present(win);
}
