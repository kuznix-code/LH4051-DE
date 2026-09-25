#include "longhorn.h"
#include "LH4051-FM/fm.h"
#include "LH4051-WM/wm.h"

typedef struct {
    GtkWindow *window;
    GtkListBox *list;
    GtkWidget *path_entry;
    GtkWidget *status;
    char *path;
} ExplorerState;

static void explorer_show_error(ExplorerState *state, const char *message)
{
    gtk_label_set_text(GTK_LABEL(state->status), message);
}

static void explorer_clear_list(GtkListBox *list)
{
    GtkWidget *child;

    while ((child = gtk_widget_get_first_child(GTK_WIDGET(list))) != NULL)
        gtk_list_box_remove(list, child);
}

static GtkWidget *make_file_row(LH4051FMItem *item)
{
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *icon = gtk_label_new(item->type == G_FILE_TYPE_DIRECTORY ? "📁" : "📄");
    GtkWidget *name = gtk_label_new(item->name);

    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_widget_set_margin_top(box, 4);
    gtk_widget_set_margin_bottom(box, 4);
    gtk_label_set_xalign(GTK_LABEL(name), 0);
    gtk_widget_set_hexpand(name, TRUE);

    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), name);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);

    /* Keep the complete path on the row so activation never depends on
     * widget text or pointer lifetime. */
    g_object_set_data_full(G_OBJECT(row), "lh4051-path", g_strdup(item->path), g_free);
    g_object_set_data_full(G_OBJECT(row), "lh4051-type",
                           GINT_TO_POINTER((gint)item->type), NULL);
    return row;
}

static void explorer_open_file(ExplorerState *state, const char *path)
{
    GFile *file = g_file_new_for_path(path);
    char *uri = g_file_get_uri(file);
    GError *error = NULL;

    if (!g_app_info_launch_default_for_uri(uri, NULL, &error)) {
        explorer_show_error(state, error ? error->message : "No application is registered for this file.");
        g_clear_error(&error);
    } else {
        explorer_show_error(state, "Opened with the default application.");
    }

    g_free(uri);
    g_object_unref(file);
}

static void explorer_load_path(ExplorerState *state, const char *path)
{
    GPtrArray *items;
    GFile *file;

    if (!path || !*path)
        return;

    file = g_file_new_for_path(path);
    if (!g_file_query_exists(file, NULL)) {
        explorer_show_error(state, "Folder does not exist.");
        g_object_unref(file);
        return;
    }
    g_object_unref(file);

    g_free(state->path);
    state->path = g_canonicalize_filename(path, NULL);
    gtk_editable_set_text(GTK_EDITABLE(state->path_entry), state->path);

    explorer_clear_list(state->list);
    items = lh4051_fm_list(state->path);

    for (guint i = 0; i < items->len; i++) {
        LH4051FMItem *item = g_ptr_array_index(items, i);
        gtk_list_box_append(state->list, make_file_row(item));
    }

    gtk_label_set_text(GTK_LABEL(state->status),
                       g_strdup_printf("%u items", items->len));
    lh4051_fm_free_list(items);
}

static void explorer_row_activated(GtkListBox *list, GtkListBoxRow *row, gpointer user_data)
{
    ExplorerState *state = user_data;
    const char *path = g_object_get_data(G_OBJECT(row), "lh4051-path");
    GFileType type = (GFileType)GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(row), "lh4051-type")
    );

    (void)list;

    if (!path)
        return;

    if (type == G_FILE_TYPE_DIRECTORY)
        explorer_load_path(state, path);
    else
        explorer_open_file(state, path);
}

static void explorer_go_up(GtkButton *button, gpointer user_data)
{
    ExplorerState *state = user_data;
    char *parent;

    (void)button;

    parent = g_path_get_dirname(state->path);
    if (g_strcmp0(parent, state->path) != 0)
        explorer_load_path(state, parent);
    g_free(parent);
}

static void explorer_path_activate(GtkEntry *entry, gpointer user_data)
{
    ExplorerState *state = user_data;
    const char *path = gtk_editable_get_text(GTK_EDITABLE(entry));

    if (path && *path)
        explorer_load_path(state, path);
}

static void close_explorer(GtkButton *button, gpointer user_data)
{
    GtkWindow *window = GTK_WINDOW(user_data);
    (void)button;
    lh4051_wm_unregister(window);
    gtk_window_destroy(window);
}

void create_file_explorer(GtkApplication *app, const char *path)
{
    ExplorerState *state = g_new0(ExplorerState, 1);
    GtkWidget *window = gtk_application_window_new(app);
    GtkWindow *win = GTK_WINDOW(window);
    GtkWidget *root;
    GtkWidget *titlebar;
    GtkWidget *toolbar;
    GtkWidget *content;
    GtkWidget *filter;
    GtkWidget *right;
    GtkWidget *columns;
    GtkWidget *close_button;
    GtkWidget *title;
    GtkWidget *path_entry;
    GtkWidget *search;
    GtkWidget *status;
    const char *menus[] = {"File", "Edit", "View", "Favorites", "Tools", "Help"};
    const char *heads[] = {"Name", "Size", "Type", "Date Modified"};

    state->window = win;
    state->path = g_canonicalize_filename(path ? path : g_get_home_dir(), NULL);

    gtk_window_set_title(win, "My Documents");
    gtk_window_set_default_size(win, 980, 680);
    lh4051_wm_register(win, "My Documents");

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(root, "lh-explorer");
    gtk_window_set_child(win, root);

    titlebar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(titlebar, "lh-titlebar");
    title = gtk_label_new("My Documents");
    gtk_widget_set_hexpand(title, TRUE);
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_box_append(GTK_BOX(titlebar), title);

    close_button = gtk_button_new_with_label("✕");
    gtk_widget_add_css_class(close_button, "lh-close");
    g_signal_connect(close_button, "clicked", G_CALLBACK(close_explorer), win);
    gtk_box_append(GTK_BOX(titlebar), close_button);
    gtk_box_append(GTK_BOX(root), titlebar);

    GtkWidget *menu_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(menu_bar, "lh-menu");
    for (guint i = 0; i < G_N_ELEMENTS(menus); i++) {
        GtkWidget *button = gtk_button_new_with_label(menus[i]);
        gtk_box_append(GTK_BOX(menu_bar), button);
    }
    gtk_box_append(GTK_BOX(root), menu_bar);

    toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_add_css_class(toolbar, "lh-toolbar");

    GtkWidget *back = gtk_button_new_with_label("←");
    GtkWidget *forward = gtk_button_new_with_label("→");
    GtkWidget *up = gtk_button_new_with_label("↑");
    gtk_box_append(GTK_BOX(toolbar), back);
    gtk_box_append(GTK_BOX(toolbar), forward);
    gtk_box_append(GTK_BOX(toolbar), up);

    path_entry = gtk_entry_new();
    gtk_widget_set_hexpand(path_entry, TRUE);
    gtk_box_append(GTK_BOX(toolbar), path_entry);

    search = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search), "Search");
    gtk_widget_set_size_request(search, 190, -1);
    gtk_box_append(GTK_BOX(toolbar), search);
    gtk_box_append(GTK_BOX(root), toolbar);

    content = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(content), 220);
    gtk_widget_set_vexpand(content, TRUE);

    filter = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(filter, "lh-filter");
    gtk_box_append(GTK_BOX(filter), gtk_label_new("Filter by"));
    GtkWidget *filter_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(filter_entry), "Filter...");
    gtk_box_append(GTK_BOX(filter), filter_entry);

    const char *filters[] = {"Recent Documents", "By Modified Year", "By Type", "By Size"};
    for (guint i = 0; i < G_N_ELEMENTS(filters); i++)
        gtk_box_append(GTK_BOX(filter), gtk_button_new_with_label(filters[i]));

    right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    columns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(columns, "lh-toolbar");
    for (guint i = 0; i < G_N_ELEMENTS(heads); i++) {
        GtkWidget *button = gtk_button_new_with_label(heads[i]);
        gtk_widget_set_hexpand(button, TRUE);
        gtk_box_append(GTK_BOX(columns), button);
    }
    gtk_box_append(GTK_BOX(right), columns);

    state->list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(state->list, GTK_SELECTION_SINGLE);
    gtk_list_box_set_activate_on_single_click(state->list, FALSE);
    g_signal_connect(state->list, "row-activated",
                     G_CALLBACK(explorer_row_activated), state);
    gtk_widget_set_vexpand(GTK_WIDGET(state->list), TRUE);
    gtk_box_append(GTK_BOX(right), GTK_WIDGET(state->list));

    status = gtk_label_new("0 items");
    gtk_label_set_xalign(GTK_LABEL(status), 0);
    gtk_widget_add_css_class(status, "lh-status");
    state->status = status;
    gtk_box_append(GTK_BOX(right), status);

    gtk_paned_set_start_child(GTK_PANED(content), filter);
    gtk_paned_set_end_child(GTK_PANED(content), right);
    gtk_box_append(GTK_BOX(root), content);

    state->path_entry = path_entry;
    g_signal_connect(path_entry, "activate", G_CALLBACK(explorer_path_activate), state);
    g_signal_connect(up, "clicked", G_CALLBACK(explorer_go_up), state);

    /* GTK owns the window; keep state alive for the lifetime of the window. */
    g_object_set_data_full(G_OBJECT(win), "lh4051-explorer-state", state, (GDestroyNotify)g_free);

    explorer_load_path(state, state->path);
    gtk_window_present(win);
}
