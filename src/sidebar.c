#include "longhorn.h"
#include "LH4051-SESSION/session.h"
#include <math.h>

static GtkWidget *analog;
static GtkWidget *digital;
static GtkWidget *date_label;

/* ---------- analog clock ---------- */

static void draw_clock(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data)
{
    double cx = w / 2.0, cy = h / 2.0, r = MIN(w, h) * 0.45;
    GDateTime *now = g_date_time_new_now_local();
    double hr = g_date_time_get_hour(now) % 12, mi = g_date_time_get_minute(now);
    double se = g_date_time_get_second(now);
    double angle[3] = {(hr + mi / 60.0) * G_PI / 6 - G_PI / 2, (mi + se / 60.0) * G_PI / 30 - G_PI / 2,
                       se * G_PI / 30 - G_PI / 2};
    double length[3] = {r * 0.50, r * 0.75, r * 0.85};
    double width[3] = {4.0, 2.6, 1.2};
    cairo_pattern_t *face = cairo_pattern_create_radial(cx - r * 0.3, cy - r * 0.35, r * 0.1, cx, cy, r);
    (void)area; (void)data;

    cairo_pattern_add_color_stop_rgb(face, 0, 0.98, 0.99, 1.0);
    cairo_pattern_add_color_stop_rgb(face, 1, 0.70, 0.78, 0.86);
    cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
    cairo_set_source(cr, face);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.16, 0.24, 0.33);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    cairo_pattern_destroy(face);

    for (int i = 0; i < 12; i++) {
        double a = i * G_PI / 6, inner = r * (i % 3 == 0 ? 0.80 : 0.88);
        cairo_move_to(cr, cx + cos(a) * inner, cy + sin(a) * inner);
        cairo_line_to(cr, cx + cos(a) * r * 0.95, cy + sin(a) * r * 0.95);
        cairo_set_line_width(cr, i % 3 == 0 ? 2.4 : 1.2);
        cairo_stroke(cr);
    }
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    for (int i = 0; i < 3; i++) {
        if (i == 2) cairo_set_source_rgb(cr, 0.75, 0.15, 0.12);
        else cairo_set_source_rgb(cr, 0.12, 0.19, 0.27);
        cairo_move_to(cr, cx, cy);
        cairo_line_to(cr, cx + cos(angle[i]) * length[i], cy + sin(angle[i]) * length[i]);
        cairo_set_line_width(cr, width[i]);
        cairo_stroke(cr);
    }
    g_date_time_unref(now);
}

static void update_time(void)
{
    GDateTime *now;
    char *t, *d;
    if (!analog) return;
    now = g_date_time_new_now_local();
    t = g_date_time_format(now, lh4051_prefs_get(LH_PREF_CLOCK_24H) ? "%H:%M" : "%-I:%M %p");
    d = g_date_time_format(now, "%A, %B %-d");
    gtk_label_set_text(GTK_LABEL(digital), t);
    gtk_label_set_text(GTK_LABEL(date_label), d);
    gtk_widget_queue_draw(analog);
    g_free(t); g_free(d);
    g_date_time_unref(now);
}

static gboolean tick(gpointer data)
{
    (void)data;
    if (!analog) return G_SOURCE_REMOVE;
    update_time();
    return G_SOURCE_CONTINUE;
}

static void on_clock_pref(gpointer data) { (void)data; update_time(); }

/* ---------- notes ---------- */

static char *notes_path(void)
{
    char *dir = lh4051_data_dir();
    char *path = g_build_filename(dir, "notes.txt", NULL);
    g_free(dir);
    return path;
}

static guint notes_save_id;

static gboolean notes_save(gpointer data)
{
    GtkTextBuffer *buf = GTK_TEXT_BUFFER(data);
    GtkTextIter a, b;
    char *text, *path;
    notes_save_id = 0;
    gtk_text_buffer_get_bounds(buf, &a, &b);
    text = gtk_text_buffer_get_text(buf, &a, &b, FALSE);
    path = notes_path();
    if (!g_file_set_contents(path, text, -1, NULL)) g_warning("LH4051: could not save notes");
    g_free(text); g_free(path);
    return G_SOURCE_REMOVE;
}

static void notes_changed(GtkTextBuffer *buf, gpointer data)
{
    (void)data;
    if (notes_save_id) g_source_remove(notes_save_id);
    notes_save_id = g_timeout_add_seconds(1, notes_save, buf);
}

static void notes_destroyed(GtkWidget *w, gpointer data)
{
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));
    (void)data;
    if (notes_save_id) { g_source_remove(notes_save_id); notes_save(buf); }
}

/* ---------- tiles ---------- */

static GtkWidget *tile(const char *title, GtkWidget *body)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *head = gtk_label_new(title);
    gtk_widget_add_css_class(box, "lh-tile");
    gtk_widget_add_css_class(head, "lh-tile-title");
    gtk_label_set_xalign(GTK_LABEL(head), 0);
    gtk_box_append(GTK_BOX(box), head);
    gtk_widget_add_css_class(body, "lh-tile-body");
    gtk_box_append(GTK_BOX(box), body);
    return box;
}

static void search_activated(GtkSearchEntry *e, gpointer app)
{
    const char *q = gtk_editable_get_text(GTK_EDITABLE(e));
    if (q && *q) lh4051_explorer_open_search(GTK_APPLICATION(app), g_get_home_dir(), q);
}

typedef struct { GtkWidget *content; GtkWidget *button; gboolean collapsed; } SidebarState;

static SidebarState *sidebar_state;
static GtkWidget *sidebar_root;

/* Composite mode draws the sidebar over the desktop; let the desktop move the build
 * watermark out from under it (layer-shell mode shrinks the desktop surface instead). */
void lh4051_sidebar_sync_desktop(gboolean animate)
{
    int inset = 0;
    if (!lh4051_session_uses_layer_shell() && sidebar_state && sidebar_root &&
        lh4051_prefs_get(LH_PREF_SIDEBAR) && gtk_widget_get_visible(sidebar_root))
        inset = sidebar_state->collapsed ? LH4051_SIDEBAR_COLLAPSED
                                         : LH4051_SIDEBAR_WIDTH + LH4051_SIDEBAR_COLLAPSED;
    lh4051_desktop_set_sidebar_inset(inset, animate);
}

static void toggle_collapse(GtkButton *b, gpointer data)
{
    SidebarState *s = data;
    GtkWidget *root = gtk_widget_get_ancestor(GTK_WIDGET(b), GTK_TYPE_BOX);
    GtkRoot *top = gtk_widget_get_root(GTK_WIDGET(b));
    s->collapsed = !s->collapsed;
    gtk_widget_set_visible(s->content, !s->collapsed);
    gtk_button_set_icon_name(b, s->collapsed ? "go-previous-symbolic" : "go-next-symbolic");
    gtk_widget_set_tooltip_text(GTK_WIDGET(b), s->collapsed ? "Expand Sidebar" : "Collapse Sidebar");
    if (root)
        gtk_widget_set_size_request(root, s->collapsed ? LH4051_SIDEBAR_COLLAPSED : LH4051_SIDEBAR_WIDTH + LH4051_SIDEBAR_COLLAPSED, 1);
    /* Layer-shell surfaces are sized by their content; ask the toplevel to re-fit. */
    if (top && GTK_IS_WINDOW(top) && lh4051_session_uses_layer_shell())
        gtk_window_set_default_size(GTK_WINDOW(top), s->collapsed ? LH4051_SIDEBAR_COLLAPSED : LH4051_SIDEBAR_WIDTH + LH4051_SIDEBAR_COLLAPSED, -1);
    lh4051_sidebar_sync_desktop(TRUE);
}

static void on_sidebar_destroy(GtkWidget *w, gpointer data)
{
    (void)w;
    if (sidebar_state == data) { sidebar_state = NULL; sidebar_root = NULL; }
    g_free(data);
    analog = digital = date_label = NULL;
}

GtkWidget *lh4051_sidebar_new(GtkApplication *app)
{
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *handle = gtk_button_new_from_icon_name("go-next-symbolic");
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *search = gtk_search_entry_new();
    GtkWidget *clock_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *notes = gtk_text_view_new();
    GtkWidget *notes_scroll = gtk_scrolled_window_new();
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(notes));
    SidebarState *state = g_new0(SidebarState, 1);
    char *path = notes_path(), *saved = NULL;

    gtk_widget_add_css_class(root, "lh-sidebar");
    gtk_widget_set_size_request(root, LH4051_SIDEBAR_WIDTH + LH4051_SIDEBAR_COLLAPSED, 1);

    state->content = content;
    gtk_widget_add_css_class(handle, "lh-sidebar-handle");
    gtk_widget_set_tooltip_text(handle, "Collapse Sidebar");
    gtk_widget_set_vexpand(handle, TRUE);
    g_signal_connect(handle, "clicked", G_CALLBACK(toggle_collapse), state);
    gtk_box_append(GTK_BOX(root), handle);

    gtk_widget_set_hexpand(content, TRUE);
    gtk_box_append(GTK_BOX(root), content);

    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(search), "Search files");
    g_signal_connect(search, "activate", G_CALLBACK(search_activated), app);
    gtk_box_append(GTK_BOX(content), tile("Search", search));

    analog = gtk_drawing_area_new();
    gtk_widget_set_size_request(analog, -1, 150);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(analog), draw_clock, NULL, NULL);
    digital = gtk_label_new("");
    date_label = gtk_label_new("");
    gtk_widget_add_css_class(digital, "lh-digital");
    gtk_widget_add_css_class(date_label, "lh-date");
    gtk_box_append(GTK_BOX(clock_box), analog);
    gtk_box_append(GTK_BOX(clock_box), digital);
    gtk_box_append(GTK_BOX(clock_box), date_label);
    gtk_box_append(GTK_BOX(content), tile("Clock", clock_box));

    if (g_file_get_contents(path, &saved, NULL, NULL)) {
        gtk_text_buffer_set_text(buf, saved, -1);
        g_free(saved);
    }
    g_free(path);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(notes), GTK_WRAP_WORD_CHAR);
    gtk_widget_add_css_class(notes, "lh-notes");
    gtk_widget_set_size_request(notes_scroll, -1, 120);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(notes_scroll), notes);
    g_signal_connect(buf, "changed", G_CALLBACK(notes_changed), NULL);
    g_signal_connect(notes, "destroy", G_CALLBACK(notes_destroyed), NULL);
    {
        GtkWidget *t = tile("Notes", notes_scroll);
        gtk_widget_set_vexpand(t, TRUE);
        gtk_box_append(GTK_BOX(content), t);
    }

    sidebar_state = state;
    sidebar_root = root;
    g_signal_connect(root, "destroy", G_CALLBACK(on_sidebar_destroy), state);
    lh4051_prefs_watch(on_clock_pref, NULL);
    update_time();
    g_timeout_add_seconds(1, tick, NULL);
    return root;
}
