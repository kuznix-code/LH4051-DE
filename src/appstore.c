#include "longhorn.h"
#ifdef G_OS_UNIX
#include <gio/gdesktopappinfo.h>
#include <sys/wait.h>
#endif

typedef struct {
    char *id;
    char *name;
    char *summary;
    char *package;
    char *homepage;
    char *icon;
} LHAppInfo;

static void app_info_free(LHAppInfo *info)
{
    if (!info) return;
    g_free(info->id); g_free(info->name); g_free(info->summary);
    g_free(info->package); g_free(info->homepage); g_free(info->icon);
    g_free(info);
}

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

static const char *detect_package_manager(void)
{
    if (g_find_program_in_path("pkcon")) return "packagekit";
    if (g_find_program_in_path("pacman")) return "pacman";
    if (g_find_program_in_path("apt")) return "apt";
    if (g_find_program_in_path("dnf")) return "dnf";
    if (g_find_program_in_path("zypper")) return "zypper";
    return NULL;
}

static void show_message(GtkWindow *parent, const char *title, const char *message)
{
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", title);
    gtk_alert_dialog_set_detail(dialog, "%s", message ? message : "");
    gtk_alert_dialog_show(dialog, parent);
}

static void package_command(const char *action, const char *package)
{
    if (!package || !*package) return;

    const char *pm = detect_package_manager();
    if (!pm) return;

    const char *argv[6] = {0};
    if (g_strcmp0(pm, "packagekit") == 0) {
        argv[0] = "pkcon";
        argv[1] = g_strcmp0(action, "install") == 0 ? "install" : "remove";
        argv[2] = package;
    } else {
        argv[0] = "pkexec";
        if (g_strcmp0(pm, "pacman") == 0) {
            argv[1] = "pacman"; argv[2] = g_strcmp0(action, "install") == 0 ? "-S" : "-R";
            argv[3] = "--noconfirm"; argv[4] = package;
        } else if (g_strcmp0(pm, "apt") == 0) {
            argv[1] = "apt"; argv[2] = g_strcmp0(action, "install") == 0 ? "install" : "remove";
            argv[3] = "-y"; argv[4] = package;
        } else if (g_strcmp0(pm, "dnf") == 0) {
            argv[1] = "dnf"; argv[2] = g_strcmp0(action, "install") == 0 ? "install" : "remove";
            argv[3] = "-y"; argv[4] = package;
        } else {
            argv[1] = "zypper"; argv[2] = g_strcmp0(action, "install") == 0 ? "install" : "remove";
            argv[3] = "-y"; argv[4] = package;
        }
    }

    GError *error = NULL;
    GSubprocess *process = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_NONE, &error);
    if (!process) {
        g_warning("LH4051 App Store: %s", error ? error->message : "package manager unavailable");
        g_clear_error(&error);
        return;
    }
    g_subprocess_wait_async(process, NULL, NULL, NULL);
    g_object_unref(process);
}

static void package_confirm_finished(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GtkAlertDialog *dialog = GTK_ALERT_DIALOG(source);
    GError *error = NULL;
    int choice = gtk_alert_dialog_choose_finish(dialog, result, &error);
    if (!error && choice == 1)
        package_command(g_object_get_data(G_OBJECT(dialog), "lh-action-copy"),
                        g_object_get_data(G_OBJECT(dialog), "lh-package-copy"));
    g_clear_error(&error);
    g_object_unref(dialog);
    (void)user_data;
}

static void confirm_package_action(GtkButton *button, gpointer data)
{
    const char *action = g_object_get_data(G_OBJECT(button), "lh-action");
    const char *package = g_object_get_data(G_OBJECT(button), "lh-package");
    GtkWindow *parent = GTK_WINDOW(data);
    if (!action || !package || !*package) return;

    char *title = g_strdup_printf("%s application?", g_strcmp0(action, "install") == 0 ? "Install" : "Uninstall");
    char *detail = g_strdup_printf("Package: %s\nPackage manager: %s\n\nThis operation may require administrator authentication.",
                                    package, detect_package_manager() ? detect_package_manager() : "none");
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", title);
    gtk_alert_dialog_set_detail(dialog, "%s", detail);
    const char *buttons[] = {"Cancel", g_strcmp0(action, "install") == 0 ? "Install" : "Uninstall", NULL};
    gtk_alert_dialog_set_buttons(dialog, buttons);
    gtk_alert_dialog_set_default_button(dialog, 1);
    gtk_alert_dialog_set_cancel_button(dialog, 0);
    g_object_set_data_full(G_OBJECT(dialog), "lh-action-copy", g_strdup(action), g_free);
    g_object_set_data_full(G_OBJECT(dialog), "lh-package-copy", g_strdup(package), g_free);
    gtk_alert_dialog_choose(dialog, parent, NULL,
        (GAsyncReadyCallback)package_confirm_finished, NULL);
    g_free(title);
    g_free(detail);
}

static void launch_app(GtkButton *button, gpointer data)
{
#ifdef G_OS_UNIX
    const char *id = g_object_get_data(G_OBJECT(button), "lh-app-id");
    GAppInfo *info = id ? G_APP_INFO(g_desktop_app_info_new(id)) : NULL;
    GError *error = NULL;
    (void)data;
    if (!info) return;
    if (!g_app_info_launch(info, NULL, NULL, &error)) {
        g_warning("LH4051 App Store: %s", error ? error->message : "application launch failed");
        g_clear_error(&error);
    }
    g_object_unref(info);
#else
    (void)button; (void)data;
#endif
}

static void app_about(GtkButton *button, gpointer data)
{
    LHAppInfo *info = g_object_get_data(G_OBJECT(button), "lh-app-info");
    if (!info) return;
    GtkWindow *parent = GTK_WINDOW(data);
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", info->name ? info->name : "Application");
    char *detail = g_strdup_printf("%s\n\nIdentifier: %s\nPackage: %s\nHomepage: %s",
        info->summary ? info->summary : "No description available.",
        info->id ? info->id : "unknown",
        info->package ? info->package : "unknown",
        info->homepage ? info->homepage : "not provided");
    gtk_alert_dialog_set_detail(dialog, "%s", detail);
    gtk_alert_dialog_show(dialog, parent);
    g_free(detail);
}

static GtkWidget *make_app_card(LHAppInfo *info, GtkWindow *parent, gboolean installed)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *title = gtk_label_new(info->name ? info->name : "Application");
    GtkWidget *summary = gtk_label_new(info->summary ? info->summary : "");
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    gtk_widget_add_css_class(card, "lh-store-app-card");
    gtk_widget_add_css_class(title, "lh-store-app-name");
    gtk_widget_add_css_class(summary, "lh-store-app-summary");
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_label_set_xalign(GTK_LABEL(summary), 0);
    gtk_label_set_wrap(GTK_LABEL(summary), TRUE);

    gtk_box_append(GTK_BOX(card), title);
    gtk_box_append(GTK_BOX(card), summary);

    GtkWidget *about = gtk_button_new_with_label("About");
    g_object_set_data(G_OBJECT(about), "lh-app-info", info);
    g_signal_connect(about, "clicked", G_CALLBACK(app_about), parent);
    gtk_box_append(GTK_BOX(actions), about);

    if (installed && info->id) {
        GtkWidget *launch = gtk_button_new_with_label("Open");
        g_object_set_data_full(G_OBJECT(launch), "lh-app-id", g_strdup(info->id), g_free);
        g_signal_connect(launch, "clicked", G_CALLBACK(launch_app), NULL);
        gtk_box_append(GTK_BOX(actions), launch);
    }

    if (info->package) {
        GtkWidget *action = gtk_button_new_with_label(installed ? "Uninstall" : "Install");
        g_object_set_data(G_OBJECT(action), "lh-action", installed ? "remove" : "install");
        g_object_set_data_full(G_OBJECT(action), "lh-package", g_strdup(info->package), g_free);
        g_signal_connect(action, "clicked", G_CALLBACK(confirm_package_action), parent);
        gtk_box_append(GTK_BOX(actions), action);
    }

    gtk_box_append(GTK_BOX(card), actions);
    return card;
}

static LHAppInfo *app_info_from_appstream(const char *block)
{
    LHAppInfo *info = g_new0(LHAppInfo, 1);
    gchar **lines = g_strsplit(block, "\n", -1);
    for (guint i = 0; lines[i]; i++) {
        char *line = g_strstrip(lines[i]);
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = 0;
        char *value = g_strstrip(colon + 1);
        if (g_strcmp0(line, "Identifier") == 0) info->id = g_strdup(value);
        else if (g_strcmp0(line, "Name") == 0) info->name = g_strdup(value);
        else if (g_strcmp0(line, "Summary") == 0) info->summary = g_strdup(value);
        else if (g_strcmp0(line, "Package") == 0) info->package = g_strdup(value);
        else if (g_strcmp0(line, "Homepage") == 0) info->homepage = g_strdup(value);
        else if (g_strcmp0(line, "Icon") == 0) info->icon = g_strdup(value);
    }
    g_strfreev(lines);
    if (!info->name && !info->id) { app_info_free(info); return NULL; }
    return info;
}

static void populate_search_results(GtkWidget *flow, const char *output, GtkWindow *parent)
{
    gchar **blocks = g_strsplit(output ? output : "", "\n---\n", -1);
    for (guint i = 0; blocks[i]; i++) {
        LHAppInfo *info = app_info_from_appstream(blocks[i]);
        if (!info) continue;
        GtkWidget *card = make_app_card(info, parent, FALSE);
        g_object_set_data_full(G_OBJECT(card), "lh-app-info", info, (GDestroyNotify)app_info_free);
        gtk_flow_box_insert(GTK_FLOW_BOX(flow), card, -1);
    }
    g_strfreev(blocks);
}

static void search_store(GtkButton *button, gpointer data)
{
    GtkWidget *entry = g_object_get_data(G_OBJECT(button), "lh-entry");
    GtkWidget *flow = g_object_get_data(G_OBJECT(button), "lh-results");
    const char *query = gtk_editable_get_text(GTK_EDITABLE(entry));
    (void)data;

    while (gtk_widget_get_first_child(flow))
        gtk_flow_box_remove(GTK_FLOW_BOX(flow), gtk_widget_get_first_child(flow));

    if (!query || !*query) return;

    const gchar *argv[] = {"appstreamcli", "search", query, NULL};
    GError *error = NULL;
    GSubprocess *process = g_subprocess_newv(
        argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &error);
    if (!process) {
        show_message(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))),
                     "AppStream unavailable",
                     error ? error->message : "appstreamcli was not found.");
        g_clear_error(&error);
        return;
    }

    gchar *out = NULL, *err = NULL;
    if (g_subprocess_communicate_utf8(process, NULL, NULL, &out, &err, &error)) {
        populate_search_results(flow, out, GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))));
        if (!out || !*out)
            show_message(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))),
                         "No results", "AppStream did not return any matching applications.");
    } else {
        show_message(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))),
                     "Search failed", error ? error->message : "AppStream search failed.");
    }
    g_free(out); g_free(err); g_clear_error(&error);
    g_object_unref(process);
}

static void populate_installed_apps(GtkWidget *flow, GtkWindow *parent)
{
#ifdef G_OS_UNIX
    GList *apps = g_app_info_get_all();
    for (GList *l = apps; l; l = l->next) {
        GAppInfo *ginfo = l->data;
        if (!g_app_info_should_show(ginfo) || !g_app_info_get_display_name(ginfo)) continue;
        LHAppInfo *info = g_new0(LHAppInfo, 1);
        info->id = g_strdup(g_app_info_get_id(ginfo));
        info->name = g_strdup(g_app_info_get_display_name(ginfo));
        info->summary = g_strdup(g_app_info_get_description(ginfo));
        info->package = g_strdup(info->id);
        GtkWidget *card = make_app_card(info, parent, TRUE);
        g_object_set_data_full(G_OBJECT(card), "lh-app-info", info, (GDestroyNotify)app_info_free);
        gtk_flow_box_insert(GTK_FLOW_BOX(flow), card, -1);
    }
    g_list_free_full(apps, g_object_unref);
#else
    gtk_flow_box_insert(GTK_FLOW_BOX(flow), gtk_label_new("Installed application discovery is unavailable."), -1);
#endif
}

void create_lh4051_appstore(GtkApplication *app)
{
    GtkWidget *window_widget = gtk_application_window_new(app);
    GtkWindow *win = GTK_WINDOW(window_widget);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *search_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *entry = gtk_search_entry_new();
    GtkWidget *search = gtk_button_new_with_label("Search");
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *results_scroll = gtk_scrolled_window_new();
    GtkWidget *results = gtk_flow_box_new();
    GtkWidget *apps_scroll = gtk_scrolled_window_new();
    GtkWidget *apps = gtk_flow_box_new();

    gtk_window_set_title(win, "LH4051 App Store");
    gtk_window_set_default_size(win, 1000, 720);
    gtk_widget_add_css_class(root, "lh-store");
    gtk_window_set_child(win, root);

    GtkWidget *title = gtk_label_new("LH4051 App Store");
    GtkWidget *subtitle = gtk_label_new("Search AppStream applications and manage packages");
    gtk_widget_add_css_class(title, "lh-store-title");
    gtk_widget_add_css_class(subtitle, "lh-store-subtitle");
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_label_set_xalign(GTK_LABEL(subtitle), 0);
    gtk_box_append(GTK_BOX(header), title);
    gtk_box_append(GTK_BOX(header), subtitle);

    gtk_widget_set_hexpand(entry, TRUE);
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(entry),
                                          "Search applications...");
    gtk_box_append(GTK_BOX(search_row), entry);
    gtk_box_append(GTK_BOX(search_row), search);
    gtk_box_append(GTK_BOX(header), search_row);
    gtk_box_append(GTK_BOX(root), header);

    gtk_widget_set_vexpand(content, TRUE);
    gtk_box_append(GTK_BOX(root), content);

    GtkWidget *rlabel = gtk_label_new("AppStream results");
    gtk_label_set_xalign(GTK_LABEL(rlabel), 0);
    gtk_box_append(GTK_BOX(content), rlabel);
    gtk_widget_set_vexpand(results_scroll, TRUE);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(results), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(results), 3);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(results), 1);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(results_scroll), results);
    gtk_box_append(GTK_BOX(content), results_scroll);

    GtkWidget *ilabel = gtk_label_new("Installed applications");
    gtk_label_set_xalign(GTK_LABEL(ilabel), 0);
    gtk_box_append(GTK_BOX(content), ilabel);
    gtk_widget_set_vexpand(apps_scroll, TRUE);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(apps), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(apps), 6);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(apps_scroll), apps);
    gtk_box_append(GTK_BOX(content), apps_scroll);

    g_object_set_data(G_OBJECT(search), "lh-entry", entry);
    g_object_set_data(G_OBJECT(search), "lh-results", results);
    g_signal_connect(search, "clicked", G_CALLBACK(search_store), NULL);
    g_signal_connect(entry, "activate", G_CALLBACK(search_store), search);

    populate_installed_apps(apps, win);
    gtk_window_present(win);
}
