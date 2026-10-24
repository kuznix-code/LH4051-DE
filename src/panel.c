#include "longhorn.h"
#include "LH4051-WM/wm.h"
#include "LH4051-SESSION/session.h"

static GtkWidget *clock_label;
static GtkWidget *task_box;

static void update_clock_label(void)
{
    GDateTime *now;
    char *text, *tip;
    if (!clock_label) return;
    now = g_date_time_new_now_local();
    text = g_date_time_format(now, lh4051_prefs_get(LH_PREF_CLOCK_24H) ? "%H:%M" : "%-I:%M %p");
    tip = g_date_time_format(now, "%A, %B %-d, %Y");
    gtk_label_set_text(GTK_LABEL(clock_label), text);
    gtk_widget_set_tooltip_text(clock_label, tip);
    g_free(text); g_free(tip);
    g_date_time_unref(now);
}

static gboolean tick(gpointer data)
{
    (void)data;
    if (!clock_label) return G_SOURCE_REMOVE;
    update_clock_label();
    return G_SOURCE_CONTINUE;
}

static void on_clock_pref(gpointer data) { (void)data; update_clock_label(); }

static void task_clicked(GtkButton *b, gpointer data)
{
    (void)b;
    lh4051_wm_toggle(GTK_WINDOW(data));
}

static void rebuild_tasks(void *data)
{
    GtkWidget *child;
    (void)data;
    if (!task_box) return;
    while ((child = gtk_widget_get_first_child(task_box)) != NULL)
        gtk_box_remove(GTK_BOX(task_box), child);

    for (guint i = 0; i < lh4051_wm_count(); i++) {
        GtkWindow *w = lh4051_wm_get_window(i);
        const char *title = lh4051_wm_get_title(i);
        GtkWidget *b = gtk_button_new_with_label(title ? title : "LH4051 Window");
        GtkWidget *label = gtk_button_get_child(GTK_BUTTON(b));
        gtk_widget_add_css_class(b, "lh-task");
        if (lh4051_wm_is_active(i)) gtk_widget_add_css_class(b, "active");
        if (GTK_IS_LABEL(label)) {
            gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
            gtk_label_set_max_width_chars(GTK_LABEL(label), 22);
        }
        g_signal_connect(b, "clicked", G_CALLBACK(task_clicked), w);
        gtk_box_append(GTK_BOX(task_box), b);
    }
}

static void start_clicked(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    lh4051_session_toggle_start_menu();
}

static void explorer_clicked(GtkButton *b, gpointer d)
{
    (void)b;
    lh4051_explorer_open(GTK_APPLICATION(d), g_get_home_dir());
}

static void on_panel_destroy(GtkWidget *w, gpointer d)
{
    (void)w; (void)d;
    clock_label = NULL;
    task_box = NULL;
    lh4051_wm_set_changed_callback(NULL, NULL);
}

static GtkWidget *tray_icon(const char *name, const char *tip)
{
    GtkWidget *img = gtk_image_new_from_icon_name(name);
    gtk_widget_add_css_class(img, "lh-tray-icon");
    gtk_widget_set_tooltip_text(img, tip);
    return img;
}

GtkWidget *lh4051_panel_new(GtkApplication *app)
{
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *start = gtk_button_new();
    GtkWidget *start_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *orb = gtk_label_new("");
    GtkWidget *quick = gtk_button_new_from_icon_name("system-file-manager");
    GtkWidget *tray = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    gtk_widget_add_css_class(bar, "lh-panel");

    gtk_widget_add_css_class(start, "lh-start-btn");
    gtk_widget_add_css_class(orb, "lh-orb");
    gtk_box_append(GTK_BOX(start_box), orb);
    gtk_box_append(GTK_BOX(start_box), gtk_label_new("start"));
    gtk_button_set_child(GTK_BUTTON(start), start_box);
    g_signal_connect(start, "clicked", G_CALLBACK(start_clicked), NULL);
    gtk_box_append(GTK_BOX(bar), start);

    gtk_widget_add_css_class(quick, "lh-quick");
    gtk_widget_set_tooltip_text(quick, "Open My Documents");
    g_signal_connect(quick, "clicked", G_CALLBACK(explorer_clicked), app);
    gtk_box_append(GTK_BOX(bar), quick);

    task_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(task_box, TRUE);
    gtk_box_append(GTK_BOX(bar), task_box);

    gtk_widget_add_css_class(tray, "lh-tray");
    gtk_box_append(GTK_BOX(tray), tray_icon("audio-volume-high-symbolic", "Volume"));
    gtk_box_append(GTK_BOX(tray), tray_icon("network-wired-symbolic", "Network"));
    clock_label = gtk_label_new("");
    gtk_widget_add_css_class(clock_label, "lh-clock");
    gtk_box_append(GTK_BOX(tray), clock_label);
    gtk_box_append(GTK_BOX(bar), tray);

    lh4051_wm_set_changed_callback(rebuild_tasks, NULL);
    lh4051_prefs_watch(on_clock_pref, NULL);
    g_signal_connect(bar, "destroy", G_CALLBACK(on_panel_destroy), NULL);
    rebuild_tasks(NULL);
    update_clock_label();
    g_timeout_add_seconds(1, tick, NULL);
    return bar;
}
