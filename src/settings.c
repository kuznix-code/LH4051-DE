#include "longhorn.h"
#include "LH4051-WM/wm.h"
#include "LH4051-SESSION/session.h"

typedef struct {
    GtkWidget *wallpaper_label;
    GtkApplication *app;
    GtkWidget *upd_note, *upd_testing, *upd_controls, *upd_status;
} SettingsState;

static GtkWindow *existing; /* single instance */

static void on_settings_destroy(GtkWidget *w, gpointer d)
{
    (void)w; (void)d;
    existing = NULL;
}

static GtkWidget *card(const char *title, const char *description)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *heading = gtk_label_new(title);
    gtk_widget_add_css_class(box, "lh-card");
    gtk_widget_add_css_class(heading, "lh-card-title");
    gtk_label_set_xalign(GTK_LABEL(heading), 0);
    gtk_box_append(GTK_BOX(box), heading);
    if (description) {
        GtkWidget *text = gtk_label_new(description);
        gtk_widget_add_css_class(text, "lh-card-desc");
        gtk_label_set_xalign(GTK_LABEL(text), 0);
        gtk_label_set_wrap(GTK_LABEL(text), TRUE);
        gtk_box_append(GTK_BOX(box), text);
    }
    return box;
}

static void pref_toggled(GtkCheckButton *b, gpointer data)
{
    lh4051_prefs_set((LHPref)GPOINTER_TO_INT(data), gtk_check_button_get_active(b));
}

static GtkWidget *pref_check(const char *label, LHPref key)
{
    GtkWidget *c = gtk_check_button_new_with_label(label);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(c), lh4051_prefs_get(key));
    g_signal_connect(c, "toggled", G_CALLBACK(pref_toggled), GINT_TO_POINTER(key));
    return c;
}

static void set_wallpaper_text(SettingsState *s)
{
    const char *p = lh4051_prefs_wallpaper();
    gtk_label_set_text(GTK_LABEL(s->wallpaper_label), p ? p : "Default (Longhorn Aurora)");
}

static void wallpaper_chosen(GObject *src, GAsyncResult *res, gpointer data)
{
    SettingsState *s = data;
    GError *error = NULL;
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(src), res, &error);
    if (file) {
        char *path = g_file_get_path(file);
        if (path) { lh4051_prefs_set_wallpaper(path); if (existing) set_wallpaper_text(s); }
        g_free(path);
        g_object_unref(file);
    }
    g_clear_error(&error); /* dismissed dialog */
    g_object_unref(src);
}

static void browse_wallpaper(GtkButton *b, gpointer data)
{
    GtkFileDialog *dialog = gtk_file_dialog_new();
    GtkFileFilter *filter = gtk_file_filter_new();
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    const char *suffixes[] = {"jpg", "jpeg", "png", "bmp", "webp", "tif", "tiff", NULL};

    gtk_file_filter_set_name(filter, "Images");
    for (int i = 0; suffixes[i]; i++) gtk_file_filter_add_suffix(filter, suffixes[i]);
    g_list_store_append(filters, filter);
    g_object_unref(filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filters);
    gtk_file_dialog_set_title(dialog, "Choose wallpaper");
    gtk_file_dialog_open(dialog, GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(b))), NULL, wallpaper_chosen, data);
}

static void reset_wallpaper(GtkButton *b, gpointer data)
{
    (void)b;
    lh4051_prefs_set_wallpaper(NULL);
    set_wallpaper_text(data);
}

/* ---- Updates card ---------------------------------------------------------------------------------- */

static void updater_refresh(SettingsState *s)
{
    char *note = NULL;
    LHUpdaterPolicy policy = lh4051_updater_policy(&note);
    gtk_label_set_text(GTK_LABEL(s->upd_note), note);
    g_free(note);
    gtk_widget_set_visible(s->upd_controls, policy != LH_UPDATER_DISABLED);
    /* The testing switch only matters where the updater is off by default. */
    gtk_widget_set_visible(s->upd_testing, policy != LH_UPDATER_DEFAULT);
}

static void updater_testing_toggled(GtkCheckButton *b, gpointer data)
{
    lh4051_prefs_set(LH_PREF_UPDATER_TESTING, gtk_check_button_get_active(b));
    updater_refresh(data);
}

static void updater_mode_changed(GtkDropDown *d, GParamSpec *pspec, gpointer data)
{
    const char *id = NULL;
    (void)pspec; (void)data;
    lh4051_updater_mode_info(gtk_drop_down_get_selected(d), &id, NULL);
    lh4051_updater_set_mode(id);
}

static void updater_run(SettingsState *s, gboolean install)
{
    char *problem = NULL, *detail;
    GPtrArray *argv;

    if (lh4051_updater_policy(NULL) == LH_UPDATER_DISABLED) { updater_refresh(s); return; }
    argv = lh4051_updater_argv(install, &problem);
    if (!argv) {
        gtk_label_set_text(GTK_LABEL(s->upd_status), problem ? problem : "The updater could not be started.");
        g_free(problem);
        return;
    }
    gtk_label_set_text(GTK_LABEL(s->upd_status), "");
    detail = g_strdup_printf("Update channel: %s", lh4051_updater_mode());
    lh4051_run_logged(s->app, install ? "Installing the update" : "Checking for updates", detail, argv);
    g_free(detail);
    g_ptr_array_unref(argv);
}

static void updater_check_clicked(GtkButton *b, gpointer data) { (void)b; updater_run(data, FALSE); }
static void updater_install_clicked(GtkButton *b, gpointer data) { (void)b; updater_run(data, TRUE); }

static GtkWidget *updater_card(SettingsState *s)
{
    GtkWidget *c = card("Updates", "Check for and install new versions of LH4051-DE.");
    GtkWidget *row, *label, *btn, *dropdown;
    GtkStringList *labels = gtk_string_list_new(NULL);
    const char *current = lh4051_updater_mode();
    guint selected = 0;

    s->upd_note = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(s->upd_note), 0);
    gtk_label_set_wrap(GTK_LABEL(s->upd_note), TRUE);
    gtk_box_append(GTK_BOX(c), s->upd_note);

    s->upd_testing = gtk_check_button_new_with_label("Enable the updater for testing");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(s->upd_testing), lh4051_prefs_get(LH_PREF_UPDATER_TESTING));
    g_signal_connect(s->upd_testing, "toggled", G_CALLBACK(updater_testing_toggled), s);
    gtk_box_append(GTK_BOX(c), s->upd_testing);

    s->upd_controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    label = gtk_label_new("Update channel:");
    gtk_box_append(GTK_BOX(row), label);
    for (guint i = 0; i < lh4051_updater_mode_count(); i++) {
        const char *id, *text;
        lh4051_updater_mode_info(i, &id, &text);
        gtk_string_list_append(labels, text);
        if (!g_strcmp0(id, current)) selected = i;
    }
    dropdown = gtk_drop_down_new(G_LIST_MODEL(labels), NULL);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown), selected);
    g_signal_connect(dropdown, "notify::selected", G_CALLBACK(updater_mode_changed), NULL);
    gtk_box_append(GTK_BOX(row), dropdown);
    gtk_box_append(GTK_BOX(s->upd_controls), row);

    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    btn = gtk_button_new_with_label("Check for updates");
    g_signal_connect(btn, "clicked", G_CALLBACK(updater_check_clicked), s);
    gtk_box_append(GTK_BOX(row), btn);
    btn = gtk_button_new_with_label("Install update");
    g_signal_connect(btn, "clicked", G_CALLBACK(updater_install_clicked), s);
    gtk_box_append(GTK_BOX(row), btn);
    gtk_box_append(GTK_BOX(s->upd_controls), row);

    s->upd_status = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(s->upd_status), 0);
    gtk_label_set_wrap(GTK_LABEL(s->upd_status), TRUE);
    gtk_box_append(GTK_BOX(s->upd_controls), s->upd_status);
    gtk_box_append(GTK_BOX(c), s->upd_controls);

    updater_refresh(s);
    return c;
}

void lh4051_settings_open(GtkApplication *app)
{
    if (existing) { gtk_window_present(existing); return; }

    GtkWidget *scroll = gtk_scrolled_window_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    SettingsState *state;
    GtkWidget *c, *row, *btn;
    char *text;

    state = g_new0(SettingsState, 1);
    state->app = app;
    gtk_widget_add_css_class(box, "lh-settings");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);

    c = card("Desktop", "Wallpaper, desktop icons and build watermark.");
    gtk_box_append(GTK_BOX(c), pref_check("Show desktop icons", LH_PREF_DESKTOP_ICONS));
    gtk_box_append(GTK_BOX(c), pref_check("Show Build 4051 watermark", LH_PREF_WATERMARK));
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->wallpaper_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(state->wallpaper_label), 0);
    gtk_label_set_ellipsize(GTK_LABEL(state->wallpaper_label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_hexpand(state->wallpaper_label, TRUE);
    gtk_box_append(GTK_BOX(row), state->wallpaper_label);
    btn = gtk_button_new_with_label("Browse…");
    g_signal_connect(btn, "clicked", G_CALLBACK(browse_wallpaper), state);
    gtk_box_append(GTK_BOX(row), btn);
    btn = gtk_button_new_with_label("Default");
    g_signal_connect(btn, "clicked", G_CALLBACK(reset_wallpaper), state);
    gtk_box_append(GTK_BOX(row), btn);
    gtk_box_append(GTK_BOX(c), row);
    set_wallpaper_text(state);
    gtk_box_append(GTK_BOX(box), c);

    c = card("Taskbar & Sidebar", "Taskbar clock and sidebar behavior.");
    gtk_box_append(GTK_BOX(c), pref_check("Use 24-hour clock", LH_PREF_CLOCK_24H));
    gtk_box_append(GTK_BOX(c), pref_check("Show seconds in the clock", LH_PREF_CLOCK_SECONDS));
    gtk_box_append(GTK_BOX(c), pref_check("Show sidebar", LH_PREF_SIDEBAR));
    gtk_box_append(GTK_BOX(box), c);

    {
        char *updater = g_find_program_in_path("lh4051-de-updater");
        if (updater) {
            gtk_box_append(GTK_BOX(box), updater_card(state));
            g_free(updater);
        }
    }

    c = card("System", "Current session information.");
    text = g_strdup_printf("LH4051-DE %s", LH4051_VERSION);
    gtk_box_append(GTK_BOX(c), gtk_label_new(text));
    g_free(text);
    text = g_strdup_printf("Display backend: %s (%s mode)", lh4051_session_backend_name(),
                           lh4051_session_uses_layer_shell() ? "layer-shell" : "composite");
    gtk_box_append(GTK_BOX(c), gtk_label_new(text));
    g_free(text);
    text = g_strdup_printf("GTK %u.%u.%u", gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version());
    gtk_box_append(GTK_BOX(c), gtk_label_new(text));
    g_free(text);
    {
        char *os = g_get_os_info(G_OS_INFO_KEY_PRETTY_NAME);
        if (os) {
            text = g_strdup_printf("OS: %s", os);
            gtk_box_append(GTK_BOX(c), gtk_label_new(text));
            g_free(text);
            g_free(os);
        }
    }
    for (GtkWidget *l = gtk_widget_get_first_child(c); l; l = gtk_widget_get_next_sibling(l))
        if (GTK_IS_LABEL(l)) gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_append(GTK_BOX(box), c);

    existing = lh4051_wm_create_window(app, "LH4051 Settings", 640, 560, scroll);
    g_object_set_data_full(G_OBJECT(existing), "lh-settings", state, g_free);
    g_signal_connect(existing, "destroy", G_CALLBACK(on_settings_destroy), NULL);
    gtk_window_present(existing);
}
