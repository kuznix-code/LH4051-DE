#include "longhorn.h"
#include "LH4051-SESSION/session.h"
#include <string.h>

static GtkWidget *stack;
static GtkWidget *search_entry;
static GtkWidget *app_list;
static GtkApplication *menu_app;

/* ---------- helpers ---------- */

static void hide_menu(void) { lh4051_session_hide_start_menu(); }

static GtkWidget *menu_button(const char *icon, const char *label, GCallback cb, gpointer data)
{
    GtkWidget *b = gtk_button_new();
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *text = gtk_label_new(label);
    if (icon) {
        GtkWidget *img = gtk_image_new_from_icon_name(icon);
        gtk_image_set_pixel_size(GTK_IMAGE(img), 24);
        gtk_box_append(GTK_BOX(row), img);
    }
    gtk_label_set_xalign(GTK_LABEL(text), 0);
    gtk_box_append(GTK_BOX(row), text);
    gtk_button_set_child(GTK_BUTTON(b), row);
    gtk_widget_add_css_class(b, "lh-start-item");
    g_signal_connect(b, "clicked", cb, data);
    return b;
}

/* ---------- actions ---------- */

static void open_dir(GUserDirectory dir)
{
    lh4051_explorer_open(menu_app, lh4051_user_dir(dir));
    hide_menu();
}
static void on_documents(GtkButton *b, gpointer d) { (void)b; (void)d; open_dir(G_USER_DIRECTORY_DOCUMENTS); }
static void on_pictures(GtkButton *b, gpointer d) { (void)b; (void)d; open_dir(G_USER_DIRECTORY_PICTURES); }
static void on_music(GtkButton *b, gpointer d) { (void)b; (void)d; open_dir(G_USER_DIRECTORY_MUSIC); }
static void on_computer(GtkButton *b, gpointer d)
{
    char *root = lh4051_computer_root();
    (void)b; (void)d;
    lh4051_explorer_open(menu_app, root);
    g_free(root);
    hide_menu();
}
static void on_settings(GtkButton *b, gpointer d) { (void)b; (void)d; lh4051_settings_open(menu_app); hide_menu(); }
static void on_store(GtkButton *b, gpointer d) { (void)b; (void)d; lh4051_appstore_open(menu_app); hide_menu(); }

static void quit_answer(GObject *src, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    int choice = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(src), res, &error);
    (void)data;
    g_clear_error(&error);
    if (choice == 1) lh4051_session_quit();
    g_object_unref(src);
}

static void on_power(GtkButton *b, gpointer data)
{
    const char *what = data;
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", what);
    const char *buttons[] = {"Cancel", what, NULL};
    GtkWindow *parent = lh4051_session_uses_layer_shell() ? NULL : GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(b)));
    gtk_alert_dialog_set_detail(dialog, "This closes the LH4051-DE shell and any windows it owns.");
    gtk_alert_dialog_set_buttons(dialog, buttons);
    gtk_alert_dialog_set_cancel_button(dialog, 0);
    gtk_alert_dialog_set_default_button(dialog, 0);
    hide_menu();
    gtk_alert_dialog_choose(dialog, parent, NULL, quit_answer, NULL);
}

static void show_all_programs(GtkButton *b, gpointer d) { (void)b; (void)d; gtk_stack_set_visible_child_name(GTK_STACK(stack), "all"); }
static void show_pinned(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    gtk_editable_set_text(GTK_EDITABLE(search_entry), "");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "pinned");
}

/* ---------- program list ---------- */

static void app_row_activated(GtkListBox *list, GtkListBoxRow *row, gpointer data)
{
    GAppInfo *info = g_object_get_data(G_OBJECT(row), "lh-app");
    GError *error = NULL;
    (void)list; (void)data;
    if (info && !g_app_info_launch(info, NULL, NULL, &error)) {
        g_warning("LH4051: cannot launch %s: %s", g_app_info_get_display_name(info),
                  error ? error->message : "unknown error");
        g_clear_error(&error);
    }
    hide_menu();
}

static gint compare_apps(gconstpointer a, gconstpointer b)
{
    return g_utf8_collate(g_app_info_get_display_name((GAppInfo *)a), g_app_info_get_display_name((GAppInfo *)b));
}

static void populate_apps(GtkWidget *list)
{
    GList *apps = g_list_sort(g_app_info_get_all(), compare_apps);
    guint count = 0;
    for (GList *l = apps; l; l = l->next) {
        GAppInfo *info = l->data;
        GtkWidget *row, *box, *img, *label;
        GIcon *icon;
        if (!g_app_info_should_show(info) || !g_app_info_get_display_name(info)) continue;
        row = gtk_list_box_row_new();
        box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        icon = g_app_info_get_icon(info);
        img = icon ? gtk_image_new_from_gicon(icon) : gtk_image_new_from_icon_name("application-x-executable");
        gtk_image_set_pixel_size(GTK_IMAGE(img), 24);
        label = gtk_label_new(g_app_info_get_display_name(info));
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_box_append(GTK_BOX(box), img);
        gtk_box_append(GTK_BOX(box), label);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        gtk_widget_add_css_class(row, "lh-start-row");
        g_object_set_data_full(G_OBJECT(row), "lh-app", g_object_ref(info), g_object_unref);
        gtk_list_box_append(GTK_LIST_BOX(list), row);
        count++;
    }
    g_list_free_full(apps, g_object_unref);
    if (!count) {
        GtkWidget *empty = gtk_label_new("No registered applications were found.");
        gtk_widget_add_css_class(empty, "lh-start-empty");
        gtk_list_box_append(GTK_LIST_BOX(list), empty);
    }
}

static gboolean app_filter(GtkListBoxRow *row, gpointer data)
{
    const char *q = gtk_editable_get_text(GTK_EDITABLE(search_entry));
    GAppInfo *info = g_object_get_data(G_OBJECT(row), "lh-app");
    char *a, *b;
    gboolean match;
    (void)data;
    if (!q || !*q || !info) return TRUE;
    a = g_utf8_casefold(g_app_info_get_display_name(info), -1);
    b = g_utf8_casefold(q, -1);
    match = strstr(a, b) != NULL;
    g_free(a); g_free(b);
    return match;
}

static void search_changed(GtkSearchEntry *e, gpointer data)
{
    const char *q = gtk_editable_get_text(GTK_EDITABLE(e));
    (void)data;
    if (q && *q) gtk_stack_set_visible_child_name(GTK_STACK(stack), "all");
    gtk_list_box_invalidate_filter(GTK_LIST_BOX(app_list));
}

void lh4051_start_menu_reset(void)
{
    if (!stack) return;
    gtk_editable_set_text(GTK_EDITABLE(search_entry), "");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "pinned");
}

static void on_start_destroy(GtkWidget *w, gpointer d)
{
    (void)w; (void)d;
    stack = search_entry = app_list = NULL;
}

/* ---------- construction ---------- */

GtkWidget *lh4051_start_menu_new(GtkApplication *app)
{
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *pinned = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *all = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *user = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *avatar, *name, *power;
    const char *real = g_get_real_name();
    char *initial;

    menu_app = app;
    gtk_widget_add_css_class(root, "lh-start-menu");
    gtk_widget_set_size_request(root, 540, 520);

    /* --- left column --- */
    gtk_widget_add_css_class(left, "lh-start-left");
    gtk_widget_set_size_request(left, 300, -1);
    gtk_widget_set_hexpand(left, FALSE);

    stack = gtk_stack_new();
    gtk_widget_set_vexpand(stack, TRUE);
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);

    gtk_box_append(GTK_BOX(pinned), menu_button("folder-documents", "My Documents", G_CALLBACK(on_documents), NULL));
    gtk_box_append(GTK_BOX(pinned), menu_button("folder-pictures", "My Pictures", G_CALLBACK(on_pictures), NULL));
    gtk_box_append(GTK_BOX(pinned), menu_button("preferences-system", "LH4051 Settings", G_CALLBACK(on_settings), NULL));
    gtk_box_append(GTK_BOX(pinned), menu_button("system-software-install", "LH4051 App Store", G_CALLBACK(on_store), NULL));
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(pinned), spacer);
    {
        GtkWidget *allb = menu_button("go-next-symbolic", "All Programs", G_CALLBACK(show_all_programs), NULL);
        gtk_widget_add_css_class(allb, "lh-start-allprograms");
        gtk_box_append(GTK_BOX(pinned), allb);
    }
    gtk_stack_add_named(GTK_STACK(stack), pinned, "pinned");

    app_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(app_list), GTK_SELECTION_NONE);
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(app_list), TRUE);
    gtk_list_box_set_filter_func(GTK_LIST_BOX(app_list), app_filter, NULL, NULL);
    g_signal_connect(app_list, "row-activated", G_CALLBACK(app_row_activated), NULL);
    populate_apps(app_list);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), app_list);
    gtk_box_append(GTK_BOX(all), scroll);
    gtk_box_append(GTK_BOX(all), menu_button("go-previous-symbolic", "Back", G_CALLBACK(show_pinned), NULL));
    gtk_stack_add_named(GTK_STACK(stack), all, "all");
    gtk_box_append(GTK_BOX(left), stack);

    search_entry = gtk_search_entry_new();
    gtk_widget_add_css_class(search_entry, "lh-start-search");
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(search_entry), "Search programs");
    g_signal_connect(search_entry, "search-changed", G_CALLBACK(search_changed), NULL);
    gtk_box_append(GTK_BOX(left), search_entry);

    /* --- right column --- */
    gtk_widget_add_css_class(right, "lh-start-right");
    gtk_widget_set_hexpand(right, TRUE);

    gtk_widget_add_css_class(user, "lh-user");
    {
        char *first = g_utf8_substring(real && *real ? real : g_get_user_name(), 0, 1);
        initial = g_utf8_strup(first, -1);
        g_free(first);
    }
    avatar = gtk_label_new(initial);
    g_free(initial);
    gtk_widget_add_css_class(avatar, "lh-avatar");
    name = gtk_label_new(real && *real ? real : g_get_user_name());
    gtk_widget_add_css_class(name, "lh-username");
    gtk_label_set_xalign(GTK_LABEL(name), 0);
    gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(user), avatar);
    gtk_box_append(GTK_BOX(user), name);
    gtk_box_append(GTK_BOX(right), user);

    gtk_box_append(GTK_BOX(right), menu_button("folder-documents", "Documents", G_CALLBACK(on_documents), NULL));
    gtk_box_append(GTK_BOX(right), menu_button("folder-pictures", "Pictures", G_CALLBACK(on_pictures), NULL));
    gtk_box_append(GTK_BOX(right), menu_button("folder-music", "Music", G_CALLBACK(on_music), NULL));
    gtk_box_append(GTK_BOX(right), menu_button("computer", "Computer", G_CALLBACK(on_computer), NULL));
    gtk_box_append(GTK_BOX(right), menu_button("preferences-system", "Control Panel", G_CALLBACK(on_settings), NULL));

    spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(right), spacer);

    power = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(power, "lh-start-power");
    {
        GtkWidget *off = gtk_button_new_with_label("Log Off");
        GtkWidget *shut = gtk_button_new_with_label("Turn Off Computer");
        gtk_widget_add_css_class(off, "lh-power-btn");
        gtk_widget_add_css_class(shut, "lh-power-btn");
        g_signal_connect(off, "clicked", G_CALLBACK(on_power), "Log Off");
        g_signal_connect(shut, "clicked", G_CALLBACK(on_power), "Turn Off Computer");
        gtk_box_append(GTK_BOX(power), off);
        gtk_box_append(GTK_BOX(power), shut);
    }
    gtk_box_append(GTK_BOX(right), power);

    gtk_box_append(GTK_BOX(root), left);
    gtk_box_append(GTK_BOX(root), right);
    g_signal_connect(root, "destroy", G_CALLBACK(on_start_destroy), NULL);
    return root;
}
