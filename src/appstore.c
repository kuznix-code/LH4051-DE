#include "longhorn.h"
#ifdef G_OS_UNIX
#include <gio/gdesktopappinfo.h>
#endif

static void close_store(GtkButton *button, gpointer data)
{
    (void)button;
    gtk_window_destroy(GTK_WINDOW(data));
}

static void append_text(GtkTextBuffer *buffer, const char *text)
{
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, text ? text : "", -1);
}

#ifdef G_OS_UNIX
static void launch_store_app(GtkButton *button, gpointer data)
{
    const char *id = g_object_get_data(G_OBJECT(button), "lh-app-id");
    GAppInfo *info = id ? G_APP_INFO(g_desktop_app_info_new(id)) : NULL;
    GError *error = NULL;
    (void)data;

    if (!info)
        return;

    if (!g_app_info_launch(info, NULL, NULL, &error)) {
        g_warning("LH4051 App Store: %s",
                  error ? error->message : "application launch failed");
        g_clear_error(&error);
    }
    g_object_unref(info);
}
#endif

static void run_package_action(const char *action, const char *package)
{
    if (!package || !*package)
        return;

    const gchar *argv[] = {"pkcon", action, package, NULL};
    GError *error = NULL;
    GSubprocess *process = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_NONE, &error);
    if (!process) {
        g_warning("LH4051 App Store: %s",
                  error ? error->message : "PackageKit unavailable");
        g_clear_error(&error);
        return;
    }
    g_subprocess_wait_async(process, NULL, NULL, NULL);
    g_object_unref(process);
}

static void package_action_from_entry(GtkButton *button, gpointer data)
{
    (void)data;
    GtkWidget *entry = g_object_get_data(G_OBJECT(button), "lh-package-entry");
    const char *action = g_object_get_data(G_OBJECT(button), "lh-package-action");
    if (entry && action)
        run_package_action(action, gtk_editable_get_text(GTK_EDITABLE(entry)));
}

static void uninstall_app(GtkButton *button, gpointer data)
{
    (void)data;
    const char *package = g_object_get_data(G_OBJECT(button), "lh-package-name");
    run_package_action("remove", package);
}

static void app_about(GtkButton *button, gpointer data)
{
    GtkWindow *parent = GTK_WINDOW(data);
    GAppInfo *info = g_object_get_data(G_OBJECT(button), "lh-app-info");
    if (!info)
        return;

    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_title(GTK_WINDOW(dialog), "Application Information");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 460, 320);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(box, 18);
    gtk_widget_set_margin_bottom(box, 18);
    gtk_widget_set_margin_start(box, 18);
    gtk_widget_set_margin_end(box, 18);
    gtk_window_set_child(GTK_WINDOW(dialog), box);

    GtkWidget *name = gtk_label_new(g_app_info_get_display_name(info));
    gtk_widget_add_css_class(name, "lh-store-about-title");
    gtk_label_set_xalign(GTK_LABEL(name), 0);
    gtk_box_append(GTK_BOX(box), name);

    const char *description = g_app_info_get_description(info);
    GtkWidget *desc = gtk_label_new(description && *description
        ? description : "No application description is available.");
    gtk_label_set_wrap(GTK_LABEL(desc), TRUE);
    gtk_label_set_xalign(GTK_LABEL(desc), 0);
    gtk_box_append(GTK_BOX(box), desc);

    const char *id = g_app_info_get_id(info);
    GtkWidget *id_label = gtk_label_new(id ? id : "Unknown desktop application ID");
    gtk_label_set_xalign(GTK_LABEL(id_label), 0);
    gtk_box_append(GTK_BOX(box), id_label);

    GtkWidget *package = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(package), id ? id : "");
    gtk_entry_set_placeholder_text(GTK_ENTRY(package), "Package name");
    gtk_box_append(GTK_BOX(box), package);

    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *install = gtk_button_new_with_label("Install");
    GtkWidget *uninstall = gtk_button_new_with_label("Uninstall");
    GtkWidget *close = gtk_button_new_with_label("Close");

    g_object_set_data(G_OBJECT(install), "lh-package-entry", package);
    g_object_set_data(G_OBJECT(install), "lh-package-action", "install");
    g_object_set_data(G_OBJECT(uninstall), "lh-package-entry", package);
    g_object_set_data(G_OBJECT(uninstall), "lh-package-action", "remove");
    g_signal_connect(install, "clicked", G_CALLBACK(package_action_from_entry), NULL);
    g_signal_connect(uninstall, "clicked", G_CALLBACK(package_action_from_entry), NULL);
    g_signal_connect_swapped(close, "clicked", G_CALLBACK(gtk_window_destroy), dialog);

    gtk_box_append(GTK_BOX(actions), install);
    gtk_box_append(GTK_BOX(actions), uninstall);
    gtk_box_append(GTK_BOX(actions), close);
    gtk_box_append(GTK_BOX(box), actions);
    gtk_window_present(GTK_WINDOW(dialog));
}

static GtkWidget *make_app_card(GAppInfo *info, GtkWindow *parent)
{
    const char *name = g_app_info_get_display_name(info);
    const char *id = g_app_info_get_id(info);
    GtkWidget *button = gtk_button_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *icon = gtk_image_new_from_gicon(g_app_info_get_icon(info));

    gtk_widget_add_css_class(button, "lh-store-app-card");
    gtk_widget_add_css_class(box, "lh-store-app-card-content");
    gtk_widget_add_css_class(icon, "lh-store-app-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 48);

    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), gtk_label_new(name ? name : "Application"));
    gtk_button_set_child(GTK_BUTTON(button), box);

#ifdef G_OS_UNIX
    if (id)
        g_object_set_data_full(G_OBJECT(button), "lh-app-id", g_strdup(id), g_free);
#endif

    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *about = gtk_button_new_with_label("About");
    GtkWidget *uninstall = gtk_button_new_with_label("Uninstall");
    gtk_widget_add_css_class(about, "lh-store-card-action");
    gtk_widget_add_css_class(uninstall, "lh-store-card-action");
    g_object_set_data(G_OBJECT(about), "lh-app-info", info);
    g_object_set_data_full(G_OBJECT(uninstall), "lh-package-name",
                           g_strdup(id ? id : ""), g_free);
    g_signal_connect(about, "clicked", G_CALLBACK(app_about), parent);
    g_signal_connect(uninstall, "clicked", G_CALLBACK(uninstall_app), NULL);
    gtk_box_append(GTK_BOX(actions), about);
    gtk_box_append(GTK_BOX(actions), uninstall);
    gtk_box_append(GTK_BOX(box), actions);

    return button;
}

static void populate_installed_apps(GtkWidget *flow, GtkWindow *parent)
{
#ifdef G_OS_UNIX
    GList *apps = g_app_info_get_all();

    for (GList *l = apps; l; l = l->next) {
        GAppInfo *info = l->data;
        if (!g_app_info_should_show(info) ||
            !g_app_info_get_display_name(info) ||
            !g_app_info_get_id(info))
            continue;

        gtk_flow_box_insert(GTK_FLOW_BOX(flow), make_app_card(info, parent), -1);
    }

    g_list_free_full(apps, g_object_unref);
#else
    gtk_flow_box_insert(GTK_FLOW_BOX(flow),
        gtk_label_new("Installed application discovery is unavailable on this platform."),
        -1);
#endif
}

static void search_store(GtkButton *button, gpointer data)
{
    GtkWidget *entry = g_object_get_data(G_OBJECT(button), "entry");
    GtkTextBuffer *buffer = g_object_get_data(G_OBJECT(button), "buffer");
    const char *query = gtk_editable_get_text(GTK_EDITABLE(entry));

    (void)data;
    gtk_text_buffer_set_text(buffer, "", -1);

    if (!query || !*query) {
        append_text(buffer, "Type an application or package name to search.");
        return;
    }

    append_text(buffer, "AppStream results\n\n");

    {
        const gchar *argv[] = {"appstreamcli", "search", query, NULL};
        GError *error = NULL;
        GSubprocess *process = g_subprocess_newv(
            argv,
            G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
            &error);

        if (process) {
            gchar *out = NULL;
            gchar *err = NULL;
            if (g_subprocess_communicate_utf8(process, NULL, NULL, &out, &err, &error) &&
                out && *out)
                append_text(buffer, out);
            else
                append_text(buffer, "No AppStream results or appstreamcli is unavailable.\n");
            g_free(out);
            g_free(err);
            g_object_unref(process);
        } else {
            append_text(buffer, error ? error->message : "AppStream search unavailable.");
            append_text(buffer, "\n");
            g_clear_error(&error);
        }
    }

    append_text(buffer, "\nPackageKit results\n\n");

    {
        const gchar *argv[] = {"pkcon", "search", "name", query, NULL};
        GError *error = NULL;
        GSubprocess *process = g_subprocess_newv(
            argv,
            G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
            &error);

        if (!process) {
            append_text(buffer, error ? error->message : "PackageKit is unavailable.");
            append_text(buffer, "\n");
            g_clear_error(&error);
        } else {
            gchar *out = NULL;
            gchar *err = NULL;
            if (g_subprocess_communicate_utf8(process, NULL, NULL, &out, &err, &error)) {
                if (out && *out)
                    append_text(buffer, out);
                else if (err && *err)
                    append_text(buffer, err);
                else
                    append_text(buffer, "No PackageKit results.");
            } else {
                append_text(buffer, error ? error->message : "PackageKit search failed.");
                g_clear_error(&error);
            }
            g_free(out);
            g_free(err);
            g_object_unref(process);
        }
    }
}

void create_lh4051_appstore(GtkApplication *app)
{
    GtkWidget *window_widget = gtk_application_window_new(app);
    GtkWindow *win = GTK_WINDOW(window_widget);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *header;
    GtkWidget *content;
    GtkWidget *search_row;
    GtkWidget *entry;
    GtkWidget *search;
    GtkWidget *results_scroll;
    GtkWidget *results;
    GtkWidget *apps_scroll;
    GtkWidget *apps;

    gtk_window_set_title(win, "LH4051 App Store");
    gtk_window_set_default_size(win, 1000, 720);
    gtk_widget_add_css_class(root, "lh-store");
    gtk_window_set_child(win, root);

    header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_add_css_class(header, "lh-store-header");

    {
        GtkWidget *title_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *title = gtk_label_new("LH4051 App Store");
        GtkWidget *subtitle = gtk_label_new("Applications, metadata and packages");

        gtk_widget_add_css_class(title, "lh-store-title");
        gtk_widget_add_css_class(subtitle, "lh-store-subtitle");
        gtk_label_set_xalign(GTK_LABEL(title), 0);
        gtk_label_set_xalign(GTK_LABEL(subtitle), 0);
        gtk_widget_set_hexpand(title_box, TRUE);
        gtk_box_append(GTK_BOX(title_box), title);
        gtk_box_append(GTK_BOX(title_box), subtitle);
        gtk_box_append(GTK_BOX(title_row), title_box);

        GtkWidget *close = gtk_button_new_with_label("✕");
        gtk_widget_add_css_class(close, "lh-close");
        g_signal_connect(close, "clicked", G_CALLBACK(close_store), window_widget);
        gtk_box_append(GTK_BOX(title_row), close);
        gtk_box_append(GTK_BOX(header), title_row);
    }

    search_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(search_row, "lh-store-search");
    entry = gtk_search_entry_new();
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_search_entry_set_placeholder_text(
        GTK_SEARCH_ENTRY(entry), "Search applications and packages");
    search = gtk_button_new_with_label("Search");
    gtk_widget_add_css_class(search, "lh-store-search-button");
    gtk_box_append(GTK_BOX(search_row), entry);
    gtk_box_append(GTK_BOX(search_row), search);
    gtk_box_append(GTK_BOX(header), search_row);
    gtk_box_append(GTK_BOX(root), header);

    content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(content, "lh-store-content");
    gtk_widget_set_vexpand(content, TRUE);
    gtk_box_append(GTK_BOX(root), content);

    {
        GtkWidget *section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        GtkWidget *label = gtk_label_new("Search results");
        gtk_widget_add_css_class(label, "lh-store-section");
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_box_append(GTK_BOX(section), label);

        results_scroll = gtk_scrolled_window_new();
        gtk_widget_set_vexpand(results_scroll, TRUE);
        results = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(results), FALSE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(results), GTK_WRAP_WORD_CHAR);
        gtk_widget_add_css_class(results, "lh-store-results");
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(results_scroll), results);
        gtk_box_append(GTK_BOX(section), results_scroll);
        gtk_box_append(GTK_BOX(content), section);
    }

    {
        GtkWidget *section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        GtkWidget *label = gtk_label_new("Installed applications");
        gtk_widget_add_css_class(label, "lh-store-section");
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_box_append(GTK_BOX(section), label);

        apps_scroll = gtk_scrolled_window_new();
        gtk_widget_set_vexpand(apps_scroll, TRUE);
        apps = gtk_flow_box_new();
        gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(apps), GTK_SELECTION_NONE);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(apps), 6);
        gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(apps), 2);
        gtk_widget_add_css_class(apps, "lh-store-app-grid");
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(apps_scroll), apps);
        gtk_box_append(GTK_BOX(section), apps_scroll);
        gtk_box_append(GTK_BOX(content), section);
        populate_installed_apps(apps, win);
    }

    {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(results));
        g_object_set_data(G_OBJECT(search), "entry", entry);
        g_object_set_data(G_OBJECT(search), "buffer", buffer);
        g_signal_connect(search, "clicked", G_CALLBACK(search_store), NULL);
        g_signal_connect(entry, "activate", G_CALLBACK(search_store), search);
        append_text(buffer, "Search AppStream and PackageKit for applications and packages.");
    }

    gtk_window_present(win);
}
