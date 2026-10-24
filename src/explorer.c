#include "longhorn.h"
#include "LH4051-FM/fm.h"
#include "LH4051-WM/wm.h"

#define COL_SIZE 90
#define COL_TYPE 150
#define COL_DATE 160

typedef struct {
    GtkApplication *app;
    GtkWindow *window;
    GtkListBox *list;
    GtkWidget *path_entry;
    GtkWidget *search_entry;
    GtkWidget *status;
    GtkWidget *phode_title;
    GtkWidget *phode_sub;
    GtkWidget *back, *forward;
    char *path;
    GPtrArray *history;
    int pos;
    gboolean show_hidden;
    LH4051FMSort sort;
    gboolean ascending;
} Explorer;

static void explorer_free(Explorer *x)
{
    g_free(x->path);
    if (x->history) g_ptr_array_unref(x->history);
    g_free(x);
}

/* ---------- listing ---------- */

static GtkWidget *cell(const char *text, int width, gboolean expand)
{
    GtkWidget *l = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(l, "lh-row-label");
    if (expand) gtk_widget_set_hexpand(l, TRUE);
    else gtk_widget_set_size_request(l, width, -1);
    return l;
}

static GtkWidget *make_row(const LH4051FMItem *item)
{
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *icon = item->icon ? gtk_image_new_from_gicon(item->icon)
                                 : gtk_image_new_from_icon_name(item->type == G_FILE_TYPE_DIRECTORY ? "folder" : "text-x-generic");
    GtkWidget *name_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    char *size = lh4051_fm_format_size(item), *date = lh4051_fm_format_date(item->modified);

    gtk_image_set_pixel_size(GTK_IMAGE(icon), 20);
    gtk_widget_set_hexpand(name_box, TRUE);
    gtk_box_append(GTK_BOX(name_box), icon);
    gtk_box_append(GTK_BOX(name_box), cell(item->display_name, 0, TRUE));
    gtk_box_append(GTK_BOX(box), name_box);
    gtk_box_append(GTK_BOX(box), cell(size, COL_SIZE, FALSE));
    gtk_box_append(GTK_BOX(box), cell(item->type_desc, COL_TYPE, FALSE));
    gtk_box_append(GTK_BOX(box), cell(date, COL_DATE, FALSE));
    g_free(size); g_free(date);

    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_widget_set_margin_top(box, 3);
    gtk_widget_set_margin_bottom(box, 3);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    g_object_set_data_full(G_OBJECT(row), "lh-path", g_strdup(item->path), g_free);
    g_object_set_data(G_OBJECT(row), "lh-dir", GINT_TO_POINTER(item->type == G_FILE_TYPE_DIRECTORY));
    return row;
}

static void update_nav(Explorer *x)
{
    gtk_widget_set_sensitive(x->back, x->pos > 0);
    gtk_widget_set_sensitive(x->forward, x->history && x->pos + 1 < (int)x->history->len);
}

/* Reload the current folder. Returns FALSE (and reports) on failure. */
static gboolean reload(Explorer *x, const char *path)
{
    GError *error = NULL;
    const char *filter = gtk_editable_get_text(GTK_EDITABLE(x->search_entry));
    GPtrArray *items = lh4051_fm_list(path, x->show_hidden, filter, x->sort, x->ascending, &error);
    GtkWidget *child;
    char *text, *base;

    if (!items) {
        gtk_label_set_text(GTK_LABEL(x->status), error ? error->message : "Cannot open this folder.");
        g_clear_error(&error);
        return FALSE;
    }

    while ((child = gtk_widget_get_first_child(GTK_WIDGET(x->list))) != NULL)
        gtk_list_box_remove(x->list, child);
    for (guint i = 0; i < items->len; i++)
        gtk_list_box_append(x->list, make_row(g_ptr_array_index(items, i)));

    text = g_strdup_printf("%u item%s", items->len, items->len == 1 ? "" : "s");
    gtk_label_set_text(GTK_LABEL(x->status), text);
    gtk_label_set_text(GTK_LABEL(x->phode_sub), text);
    g_free(text);

    base = g_path_get_basename(path);
    gtk_label_set_text(GTK_LABEL(x->phode_title), (base && *base && g_strcmp0(base, ".") != 0) ? base : path);
    lh4051_wm_set_title(x->window, gtk_label_get_text(GTK_LABEL(x->phode_title)));
    g_free(base);
    lh4051_fm_free_list(items);
    return TRUE;
}

static gboolean go(Explorer *x, const char *path, gboolean push)
{
    char *canon = g_canonicalize_filename(path, NULL);
    if (push && g_strcmp0(canon, x->path) == 0) push = FALSE;
    if (!reload(x, canon)) { g_free(canon); return FALSE; }

    g_free(x->path);
    x->path = canon;
    gtk_editable_set_text(GTK_EDITABLE(x->path_entry), x->path);

    if (push) {
        while (x->history->len > (guint)(x->pos + 1))
            g_ptr_array_remove_index(x->history, x->history->len - 1);
        g_ptr_array_add(x->history, g_strdup(x->path));
        x->pos = (int)x->history->len - 1;
    }
    update_nav(x);
    return TRUE;
}

static void refresh(Explorer *x) { if (x->path) reload(x, x->path); }

/* ---------- callbacks ---------- */

static void row_activated(GtkListBox *list, GtkListBoxRow *row, gpointer data)
{
    Explorer *x = data;
    const char *path = g_object_get_data(G_OBJECT(row), "lh-path");
    (void)list;
    if (!path) return;
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "lh-dir"))) {
        go(x, path, TRUE);
    } else {
        GFile *f = g_file_new_for_path(path);
        char *uri = g_file_get_uri(f);
        GError *error = NULL;
        if (!lh4051_open_uri_checked(uri, &error)) {
            gtk_label_set_text(GTK_LABEL(x->status), error ? error->message : "No application is registered for this file.");
            g_clear_error(&error);
        }
        g_free(uri);
        g_object_unref(f);
    }
}

static void on_back(GtkButton *b, gpointer d)
{
    Explorer *x = d; (void)b;
    if (x->pos > 0 && go(x, g_ptr_array_index(x->history, x->pos - 1), FALSE)) { x->pos--; update_nav(x); }
}
static void on_forward(GtkButton *b, gpointer d)
{
    Explorer *x = d; (void)b;
    if (x->pos + 1 < (int)x->history->len && go(x, g_ptr_array_index(x->history, x->pos + 1), FALSE)) { x->pos++; update_nav(x); }
}
static void on_up(GtkButton *b, gpointer d)
{
    Explorer *x = d;
    char *parent = g_path_get_dirname(x->path);
    (void)b;
    if (g_strcmp0(parent, x->path) != 0) go(x, parent, TRUE);
    g_free(parent);
}
static void on_path_activate(GtkEntry *e, gpointer d)
{
    const char *p = gtk_editable_get_text(GTK_EDITABLE(e));
    if (p && *p) go(d, p, TRUE);
}
static void on_search_changed(GtkSearchEntry *e, gpointer d) { (void)e; refresh(d); }

static void on_sort(GtkButton *b, gpointer d)
{
    Explorer *x = d;
    LH4051FMSort s = (LH4051FMSort)GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "lh-sort"));
    if (s == x->sort) x->ascending = !x->ascending;
    else { x->sort = s; x->ascending = TRUE; }
    refresh(x);
}

static void on_place(GtkButton *b, gpointer d)
{
    const char *path = g_object_get_data(G_OBJECT(b), "lh-place");
    if (path) go(d, path, TRUE);
}

/* ---------- menus ---------- */

typedef struct { Explorer *x; GtkWidget *popover; GCallback cb; } MenuItemData;

static void menu_item_clicked(GtkButton *b, gpointer data)
{
    MenuItemData *m = data;
    (void)b;
    gtk_popover_popdown(GTK_POPOVER(m->popover));
    ((void (*)(Explorer *))m->cb)(m->x);
}

static void add_menu_item(GtkWidget *box, GtkWidget *popover, Explorer *x, const char *label, GCallback cb)
{
    GtkWidget *b = gtk_button_new_with_label(label);
    MenuItemData *m = g_new0(MenuItemData, 1);
    m->x = x; m->popover = popover; m->cb = cb;
    gtk_widget_add_css_class(b, "flat");
    gtk_widget_add_css_class(b, "lh-menu-item");
    gtk_label_set_xalign(GTK_LABEL(gtk_button_get_child(GTK_BUTTON(b))), 0);
    g_signal_connect_data(b, "clicked", G_CALLBACK(menu_item_clicked), m, (GClosureNotify)g_free, 0);
    gtk_box_append(GTK_BOX(box), b);
}

static void m_new_window(Explorer *x) { lh4051_explorer_open(x->app, x->path); }
static void m_close(Explorer *x) { gtk_window_close(x->window); }
static void m_select_all(Explorer *x) { gtk_list_box_select_all(x->list); }
static void m_refresh(Explorer *x) { refresh(x); }
static void m_hidden(Explorer *x) { x->show_hidden = !x->show_hidden; refresh(x); }
static void m_home(Explorer *x) { go(x, g_get_home_dir(), TRUE); }
static void m_docs(Explorer *x) { go(x, lh4051_user_dir(G_USER_DIRECTORY_DOCUMENTS), TRUE); }
static void m_pics(Explorer *x) { go(x, lh4051_user_dir(G_USER_DIRECTORY_PICTURES), TRUE); }
static void m_settings(Explorer *x) { lh4051_settings_open(x->app); }

static void about_done(GObject *s, GAsyncResult *r, gpointer d)
{
    (void)d;
    gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(s), r, NULL);
    g_object_unref(s);
}
static void m_about(Explorer *x)
{
    GtkAlertDialog *dlg = gtk_alert_dialog_new("LH4051-DE Explorer");
    char *detail = g_strdup_printf("Longhorn Build 4051 style desktop environment\nVersion %s", LH4051_VERSION);
    gtk_alert_dialog_set_detail(dlg, detail);
    g_free(detail);
    gtk_alert_dialog_choose(dlg, x->window, NULL, about_done, NULL);
}

static GtkWidget *menu_button_new(Explorer *x, const char *label, const char *const *names, GCallback const *cbs, int n)
{
    GtkWidget *mb = gtk_menu_button_new();
    GtkWidget *pop = gtk_popover_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_menu_button_set_label(GTK_MENU_BUTTON(mb), label);
    gtk_widget_add_css_class(mb, "flat");
    gtk_widget_add_css_class(mb, "lh-menubtn");
    gtk_popover_set_child(GTK_POPOVER(pop), box);
    gtk_popover_set_has_arrow(GTK_POPOVER(pop), FALSE);
    for (int i = 0; i < n; i++) add_menu_item(box, pop, x, names[i], cbs[i]);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(mb), pop);
    return mb;
}

static GtkWidget *build_menubar(Explorer *x)
{
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    const char *file[] = {"New Window", "Close"};
    GCallback file_cb[] = {G_CALLBACK(m_new_window), G_CALLBACK(m_close)};
    const char *edit[] = {"Select All"};
    GCallback edit_cb[] = {G_CALLBACK(m_select_all)};
    const char *view[] = {"Show/Hide Hidden Files", "Refresh"};
    GCallback view_cb[] = {G_CALLBACK(m_hidden), G_CALLBACK(m_refresh)};
    const char *fav[] = {"Home", "Documents", "Pictures"};
    GCallback fav_cb[] = {G_CALLBACK(m_home), G_CALLBACK(m_docs), G_CALLBACK(m_pics)};
    const char *tools[] = {"LH4051 Settings"};
    GCallback tools_cb[] = {G_CALLBACK(m_settings)};
    const char *help[] = {"About"};
    GCallback help_cb[] = {G_CALLBACK(m_about)};

    gtk_widget_add_css_class(bar, "lh-menu");
    gtk_box_append(GTK_BOX(bar), menu_button_new(x, "File", file, file_cb, 2));
    gtk_box_append(GTK_BOX(bar), menu_button_new(x, "Edit", edit, edit_cb, 1));
    gtk_box_append(GTK_BOX(bar), menu_button_new(x, "View", view, view_cb, 2));
    gtk_box_append(GTK_BOX(bar), menu_button_new(x, "Favorites", fav, fav_cb, 3));
    gtk_box_append(GTK_BOX(bar), menu_button_new(x, "Tools", tools, tools_cb, 1));
    gtk_box_append(GTK_BOX(bar), menu_button_new(x, "Help", help, help_cb, 1));
    return bar;
}

/* ---------- window ---------- */

static GtkWidget *place_button(Explorer *x, const char *icon, const char *label, const char *path)
{
    GtkWidget *b = gtk_button_new();
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *img = gtk_image_new_from_icon_name(icon);
    GtkWidget *text = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(text), 0);
    gtk_box_append(GTK_BOX(row), img);
    gtk_box_append(GTK_BOX(row), text);
    gtk_button_set_child(GTK_BUTTON(b), row);
    gtk_widget_add_css_class(b, "flat");
    gtk_widget_add_css_class(b, "lh-place");
    g_object_set_data_full(G_OBJECT(b), "lh-place", g_strdup(path), g_free);
    g_signal_connect(b, "clicked", G_CALLBACK(on_place), x);
    return b;
}

static GtkWidget *column_head(Explorer *x, const char *label, LH4051FMSort sort, int width)
{
    GtkWidget *b = gtk_button_new_with_label(label);
    gtk_widget_add_css_class(b, "lh-colhead");
    gtk_label_set_xalign(GTK_LABEL(gtk_button_get_child(GTK_BUTTON(b))), 0);
    if (width) gtk_widget_set_size_request(b, width, -1);
    else gtk_widget_set_hexpand(b, TRUE);
    g_object_set_data(G_OBJECT(b), "lh-sort", GINT_TO_POINTER(sort));
    g_signal_connect(b, "clicked", G_CALLBACK(on_sort), x);
    return b;
}

void lh4051_explorer_open_search(GtkApplication *app, const char *path, const char *query)
{
    Explorer *x = g_new0(Explorer, 1);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *up = gtk_button_new_from_icon_name("go-up-symbolic");
    GtkWidget *content = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *side = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *phode = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *heads = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *scroll = gtk_scrolled_window_new();
    char *root_dir = lh4051_computer_root();

    x->app = app;
    x->history = g_ptr_array_new_with_free_func(g_free);
    x->pos = -1;
    x->ascending = TRUE;
    x->sort = LH4051_FM_SORT_NAME;

    gtk_widget_add_css_class(root, "lh-explorer");
    gtk_box_append(GTK_BOX(root), build_menubar(x));

    gtk_widget_add_css_class(toolbar, "lh-toolbar");
    x->back = gtk_button_new_from_icon_name("go-previous-symbolic");
    x->forward = gtk_button_new_from_icon_name("go-next-symbolic");
    g_signal_connect(x->back, "clicked", G_CALLBACK(on_back), x);
    g_signal_connect(x->forward, "clicked", G_CALLBACK(on_forward), x);
    g_signal_connect(up, "clicked", G_CALLBACK(on_up), x);
    gtk_box_append(GTK_BOX(toolbar), x->back);
    gtk_box_append(GTK_BOX(toolbar), x->forward);
    gtk_box_append(GTK_BOX(toolbar), up);
    x->path_entry = gtk_entry_new();
    gtk_widget_set_hexpand(x->path_entry, TRUE);
    g_signal_connect(x->path_entry, "activate", G_CALLBACK(on_path_activate), x);
    gtk_box_append(GTK_BOX(toolbar), x->path_entry);
    x->search_entry = gtk_search_entry_new();
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(x->search_entry), "Search");
    gtk_widget_set_size_request(x->search_entry, 190, -1);
    g_signal_connect(x->search_entry, "search-changed", G_CALLBACK(on_search_changed), x);
    gtk_box_append(GTK_BOX(toolbar), x->search_entry);
    gtk_box_append(GTK_BOX(root), toolbar);

    /* favorite links */
    gtk_widget_add_css_class(side, "lh-filter");
    {
        GtkWidget *t = gtk_label_new("Favorite Links");
        gtk_widget_add_css_class(t, "lh-filter-title");
        gtk_label_set_xalign(GTK_LABEL(t), 0);
        gtk_box_append(GTK_BOX(side), t);
    }
    gtk_box_append(GTK_BOX(side), place_button(x, "user-home", "Home", g_get_home_dir()));
    gtk_box_append(GTK_BOX(side), place_button(x, "user-desktop", "Desktop", lh4051_user_dir(G_USER_DIRECTORY_DESKTOP)));
    gtk_box_append(GTK_BOX(side), place_button(x, "folder-documents", "Documents", lh4051_user_dir(G_USER_DIRECTORY_DOCUMENTS)));
    gtk_box_append(GTK_BOX(side), place_button(x, "folder-download", "Downloads", lh4051_user_dir(G_USER_DIRECTORY_DOWNLOAD)));
    gtk_box_append(GTK_BOX(side), place_button(x, "folder-pictures", "Pictures", lh4051_user_dir(G_USER_DIRECTORY_PICTURES)));
    gtk_box_append(GTK_BOX(side), place_button(x, "folder-music", "Music", lh4051_user_dir(G_USER_DIRECTORY_MUSIC)));
    gtk_box_append(GTK_BOX(side), place_button(x, "folder-videos", "Videos", lh4051_user_dir(G_USER_DIRECTORY_VIDEOS)));
    gtk_box_append(GTK_BOX(side), place_button(x, "computer", "Computer", root_dir));
    g_free(root_dir);

    /* phode header */
    gtk_widget_add_css_class(phode, "lh-phode");
    x->phode_title = gtk_label_new("");
    x->phode_sub = gtk_label_new("");
    gtk_widget_add_css_class(x->phode_title, "lh-phode-title");
    gtk_widget_add_css_class(x->phode_sub, "lh-phode-sub");
    gtk_label_set_xalign(GTK_LABEL(x->phode_title), 0);
    gtk_label_set_xalign(GTK_LABEL(x->phode_sub), 0);
    gtk_box_append(GTK_BOX(phode), x->phode_title);
    gtk_box_append(GTK_BOX(phode), x->phode_sub);
    gtk_box_append(GTK_BOX(right), phode);

    gtk_widget_add_css_class(heads, "lh-colheads");
    gtk_widget_set_margin_start(heads, 8);
    gtk_widget_set_margin_end(heads, 8);
    gtk_box_append(GTK_BOX(heads), column_head(x, "Name", LH4051_FM_SORT_NAME, 0));
    gtk_box_append(GTK_BOX(heads), column_head(x, "Size", LH4051_FM_SORT_SIZE, COL_SIZE + 8));
    gtk_box_append(GTK_BOX(heads), column_head(x, "Type", LH4051_FM_SORT_TYPE, COL_TYPE + 8));
    gtk_box_append(GTK_BOX(heads), column_head(x, "Date Modified", LH4051_FM_SORT_DATE, COL_DATE + 8));
    gtk_box_append(GTK_BOX(right), heads);

    x->list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(x->list, GTK_SELECTION_MULTIPLE);
    gtk_list_box_set_activate_on_single_click(x->list, FALSE);
    g_signal_connect(x->list, "row-activated", G_CALLBACK(row_activated), x);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(x->list));
    gtk_box_append(GTK_BOX(right), scroll);

    x->status = gtk_label_new("");
    gtk_widget_add_css_class(x->status, "lh-status");
    gtk_label_set_xalign(GTK_LABEL(x->status), 0);
    gtk_box_append(GTK_BOX(right), x->status);

    gtk_paned_set_start_child(GTK_PANED(content), side);
    gtk_paned_set_end_child(GTK_PANED(content), right);
    gtk_paned_set_position(GTK_PANED(content), 190);
    gtk_paned_set_shrink_start_child(GTK_PANED(content), FALSE);
    gtk_widget_set_vexpand(content, TRUE);
    gtk_box_append(GTK_BOX(root), content);

    x->window = lh4051_wm_create_window(app, "My Documents", 1000, 680, root);
    g_object_set_data_full(G_OBJECT(x->window), "lh-explorer", x, (GDestroyNotify)explorer_free);

    if (query && *query) gtk_editable_set_text(GTK_EDITABLE(x->search_entry), query);
    go(x, (path && *path) ? path : g_get_home_dir(), TRUE);
    gtk_window_present(x->window);
}

void lh4051_explorer_open(GtkApplication *app, const char *path)
{
    lh4051_explorer_open_search(app, path, NULL);
}
