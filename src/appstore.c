#include "longhorn.h"
#ifdef G_OS_UNIX
#include <gio/gdesktopappinfo.h>
#endif

static void close_store(GtkButton *b, gpointer d)
{
    (void)b;
    gtk_window_destroy(GTK_WINDOW(d));
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
        g_warning("LH4051 App Store: %s", error ? error->message : "application launch failed");
        g_clear_error(&error);
    }
    g_object_unref(info);
}

#endif

static void populate_installed_apps(GtkWidget *box)
{
#ifdef G_OS_UNIX
    GList *apps = g_app_info_get_all();

    for (GList *l = apps; l; l = l->next) {
        GAppInfo *info = l->data;
        const char *name = g_app_info_get_display_name(info);
        const char *id = g_app_info_get_id(info);

        if (!g_app_info_should_show(info) || !name || !id)
            continue;

        GtkWidget *button = gtk_button_new_with_label(name);
        gtk_widget_add_css_class(button, "lh-store-app");
        g_object_set_data_full(G_OBJECT(button), "lh-app-id", g_strdup(id), g_free);
        g_signal_connect(button, "clicked", G_CALLBACK(launch_store_app), NULL);
        gtk_box_append(GTK_BOX(box), button);
    }

    g_list_free_full(apps, g_object_unref);
#else
    gtk_box_append(GTK_BOX(box), gtk_label_new("Installed application discovery is unavailable on this platform."));
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
        append_text(buffer, "Enter an application or package name.");
        return;
    }

    append_text(buffer, "== AppStream ==\n");

    {
        const gchar *argv[] = {"appstreamcli", "search", query, NULL};
        GError *error = NULL;
        GSubprocess *process = g_subprocess_newv(argv,
            G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &error);

        if (process) {
            gchar *out = NULL;
            gchar *err = NULL;
            if (g_subprocess_communicate_utf8(process, NULL, NULL, &out, &err, &error) &&
                out && *out)
                append_text(buffer, out);
            else
                append_text(buffer, "AppStream metadata search returned no results or appstreamcli is unavailable.\n");
            g_free(out);
            g_free(err);
            g_object_unref(process);
        } else {
            append_text(buffer, error ? error->message : "AppStream search unavailable.");
            append_text(buffer, "\n");
            g_clear_error(&error);
        }
    }

    append_text(buffer, "\n== PackageKit ==\n");

    {
        const gchar *argv[] = {"pkcon", "search", "name", query, NULL};
        GError *error = NULL;
        GSubprocess *process = g_subprocess_newv(argv,
            G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &error);

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
    GtkWidget *w = gtk_application_window_new(app);
    GtkWindow *win = GTK_WINDOW(w);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    gtk_window_set_title(win, "LH4051 App Store");
    gtk_window_set_default_size(win, 900, 680);
    gtk_widget_add_css_class(root, "lh-store");
    gtk_window_set_child(win, root);

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(bar, "lh-titlebar");
    GtkWidget *title = gtk_label_new("LH4051 App Store · AppStream + PackageKit");
    gtk_widget_set_hexpand(title, TRUE);
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_box_append(GTK_BOX(bar), title);

    GtkWidget *close = gtk_button_new_with_label("✕");
    gtk_widget_add_css_class(close, "lh-close");
    g_signal_connect(close, "clicked", G_CALLBACK(close_store), w);
    gtk_box_append(GTK_BOX(bar), close);
    gtk_box_append(GTK_BOX(root), bar);

    GtkWidget *entry = gtk_search_entry_new();
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(entry), "Search applications and packages");
    gtk_widget_set_margin_start(entry, 10);
    gtk_widget_set_margin_end(entry, 10);
    gtk_box_append(GTK_BOX(root), entry);

    GtkWidget *search = gtk_button_new_with_label("Search AppStream + PackageKit");
    gtk_box_append(GTK_BOX(root), search);

    GtkWidget *results_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(results_scroll, TRUE);
    GtkWidget *results = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(results), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(results), GTK_WRAP_WORD_CHAR);
    gtk_widget_add_css_class(results, "lh-store-results");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(results_scroll), results);
    gtk_box_append(GTK_BOX(root), results_scroll);

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(results));
    g_object_set_data(G_OBJECT(search), "entry", entry);
    g_object_set_data(G_OBJECT(search), "buffer", buffer);
    g_signal_connect(search, "clicked", G_CALLBACK(search_store), NULL);

    GtkWidget *apps_label = gtk_label_new("Installed Applications");
    gtk_widget_add_css_class(apps_label, "lh-store-section");
    gtk_label_set_xalign(GTK_LABEL(apps_label), 0);
    gtk_box_append(GTK_BOX(root), apps_label);

    GtkWidget *apps_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(apps_scroll, TRUE);
    GtkWidget *apps = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(apps_scroll), apps);
    gtk_box_append(GTK_BOX(root), apps_scroll);
    populate_installed_apps(apps);

    gtk_window_present(win);
}
