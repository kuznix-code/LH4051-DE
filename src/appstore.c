#include "longhorn.h"
#include "LH4051-WM/wm.h"
#include <string.h>

typedef struct {
    char *id, *name, *summary, *package, *homepage, *icon;
    GIcon *gicon;
    GAppInfo *app; /* installed apps only */
} StoreApp;

static void store_app_free(StoreApp *a)
{
    if (!a) return;
    g_free(a->id); g_free(a->name); g_free(a->summary);
    g_free(a->package); g_free(a->homepage); g_free(a->icon);
    g_clear_object(&a->gicon);
    g_clear_object(&a->app);
    g_free(a);
}

/* ---------- package manager ---------- */

typedef enum { PM_NONE, PM_PACKAGEKIT, PM_PACMAN, PM_APT, PM_DNF, PM_ZYPPER, PM_WINGET } PackageManager;

static PackageManager detect_pm(void)
{
    if (lh4051_platform_is_windows())
        return g_find_program_in_path("winget") ? PM_WINGET : PM_NONE;
    if (g_find_program_in_path("pkcon")) return PM_PACKAGEKIT;
    if (g_find_program_in_path("pacman")) return PM_PACMAN;
    if (g_find_program_in_path("apt")) return PM_APT;
    if (g_find_program_in_path("dnf")) return PM_DNF;
    if (g_find_program_in_path("zypper")) return PM_ZYPPER;
    return PM_NONE;
}

static const char *pm_name(PackageManager pm)
{
    static const char *names[] = {"none", "PackageKit", "pacman", "apt", "dnf", "zypper", "winget"};
    return names[pm];
}

/* Builds argv for installing/removing `package`. Returns NULL if unsupported. */
static GPtrArray *pm_command(PackageManager pm, gboolean install, const char *package)
{
    GPtrArray *a = g_ptr_array_new_with_free_func(g_free);
    const char *verb = install ? "install" : "remove";
#define ADD(s) g_ptr_array_add(a, g_strdup(s))
    switch (pm) {
    case PM_PACKAGEKIT: ADD("pkcon"); ADD("-y"); ADD(verb); ADD(package); break;
    case PM_PACMAN: ADD("pkexec"); ADD("pacman"); ADD(install ? "-S" : "-R"); ADD("--noconfirm"); ADD(package); break;
    case PM_APT: ADD("pkexec"); ADD("apt-get"); ADD(verb); ADD("-y"); ADD(package); break;
    case PM_DNF: ADD("pkexec"); ADD("dnf"); ADD(verb); ADD("-y"); ADD(package); break;
    case PM_ZYPPER: ADD("pkexec"); ADD("zypper"); ADD("--non-interactive"); ADD(verb); ADD(package); break;
    case PM_WINGET:
        ADD("winget"); ADD(install ? "install" : "uninstall"); ADD("--id"); ADD(package); ADD("-e");
        ADD("--accept-source-agreements");
        if (install) ADD("--accept-package-agreements");
        break;
    default: g_ptr_array_unref(a); return NULL;
    }
#undef ADD
    g_ptr_array_add(a, NULL);
    return a;
}

/* ---------- live operation window ---------- */

typedef struct {
    GtkWidget *output, *status, *close;
    GtkWindow *window;
    GSubprocess *process;
    GDataInputStream *stream;
    gboolean running;
    int refs; /* window + wait_async + each pending read */
} Operation;

static void op_unref(Operation *op)
{
    if (--op->refs > 0) return;
    g_clear_object(&op->process);
    g_clear_object(&op->stream);
    g_free(op);
}

static void op_append(Operation *op, const char *line)
{
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(op->output));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_buffer_insert(buf, &end, line, -1);
    gtk_text_buffer_insert(buf, &end, "\n", -1);
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(op->output), &end, 0.0, FALSE, 0.0, 1.0);
}

static void op_read_line(GObject *src, GAsyncResult *res, gpointer data);

static void op_read_next(Operation *op)
{
    op->refs++;
    g_data_input_stream_read_line_async(op->stream, G_PRIORITY_DEFAULT, NULL, op_read_line, op);
}

static void op_read_line(GObject *src, GAsyncResult *res, gpointer data)
{
    Operation *op = data;
    GError *error = NULL;
    char *line = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(src), res, NULL, &error);
    if (op->window && line) {
        char *valid = g_utf8_make_valid(line, -1);
        op_append(op, valid);
        g_free(valid);
        op_read_next(op);
    } else if (op->window && error) {
        op_append(op, error->message);
    }
    g_free(line);
    g_clear_error(&error);
    op_unref(op);
}

static void op_finished(GObject *src, GAsyncResult *res, gpointer data)
{
    Operation *op = data;
    GError *error = NULL;
    gboolean ok = g_subprocess_wait_finish(G_SUBPROCESS(src), res, &error);
    int status = (ok && g_subprocess_get_if_exited(G_SUBPROCESS(src)))
                     ? g_subprocess_get_exit_status(G_SUBPROCESS(src)) : -1;
    g_clear_error(&error);
    op->running = FALSE;
    if (op->window) {
        if (status == 0) {
            gtk_label_set_text(GTK_LABEL(op->status), "Completed successfully");
            gtk_widget_add_css_class(op->status, "lh-operation-success");
        } else {
            char *m = g_strdup_printf("Operation failed (exit status %d)", status);
            gtk_label_set_text(GTK_LABEL(op->status), m);
            gtk_widget_add_css_class(op->status, "lh-operation-failed");
            g_free(m);
        }
        gtk_widget_set_sensitive(op->close, TRUE);
    }
    op_unref(op);
}

static gboolean op_close_request(GtkWindow *w, gpointer data)
{
    Operation *op = data;
    (void)w;
    return op->running; /* keep the window while the package manager runs */
}

static void op_destroyed(GtkWidget *w, gpointer data)
{
    Operation *op = data;
    (void)w;
    op->window = NULL;
    op_unref(op);
}

static void op_close_clicked(GtkButton *b, gpointer data) { (void)b; gtk_window_close(GTK_WINDOW(data)); }

static void run_logged(GtkApplication *app, const char *title, const char *detail, GPtrArray *argv)
{
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *heading = gtk_label_new(title);
    GtkWidget *info = gtk_label_new(detail);
    GtkWidget *scroll = gtk_scrolled_window_new();
    Operation *op = g_new0(Operation, 1);
    GError *error = NULL;

    op->refs = 1; /* the window */
    op->output = gtk_text_view_new();
    op->status = gtk_label_new("Starting…");
    op->close = gtk_button_new_with_label("Close");

    gtk_widget_add_css_class(root, "lh-operation");
    gtk_widget_add_css_class(heading, "lh-operation-title");
    gtk_widget_add_css_class(info, "lh-operation-detail");
    gtk_widget_add_css_class(op->output, "lh-terminal");
    gtk_widget_add_css_class(op->status, "lh-operation-status");
    gtk_label_set_xalign(GTK_LABEL(heading), 0);
    gtk_label_set_xalign(GTK_LABEL(info), 0);
    gtk_label_set_xalign(GTK_LABEL(op->status), 0);
    gtk_label_set_wrap(GTK_LABEL(info), TRUE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(op->output), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(op->output), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(op->output), TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), op->output);
    gtk_box_append(GTK_BOX(root), heading);
    gtk_box_append(GTK_BOX(root), info);
    gtk_box_append(GTK_BOX(root), scroll);
    gtk_box_append(GTK_BOX(root), op->status);
    gtk_box_append(GTK_BOX(root), op->close);
    gtk_widget_set_margin_top(root, 14);
    gtk_widget_set_margin_bottom(root, 14);
    gtk_widget_set_margin_start(root, 14);
    gtk_widget_set_margin_end(root, 14);

    op->window = lh4051_wm_create_window(app, "LH4051 App Store — Operation", 720, 480, root);
    g_signal_connect(op->close, "clicked", G_CALLBACK(op_close_clicked), op->window);
    g_signal_connect(op->window, "close-request", G_CALLBACK(op_close_request), op);
    g_signal_connect(op->window, "destroy", G_CALLBACK(op_destroyed), op);
    gtk_window_present(op->window);

    op->process = g_subprocess_newv((const char *const *)argv->pdata,
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE, &error);
    if (!op->process) {
        op_append(op, error ? error->message : "Could not start the command.");
        gtk_label_set_text(GTK_LABEL(op->status), "Could not start the package manager");
        gtk_widget_add_css_class(op->status, "lh-operation-failed");
        g_clear_error(&error);
        return; /* window owns op; released in op_destroyed */
    }
    op->running = TRUE;
    op->refs++; /* wait_async */
    gtk_widget_set_sensitive(op->close, FALSE);
    gtk_label_set_text(GTK_LABEL(op->status), "Running…");
    op->stream = g_data_input_stream_new(g_subprocess_get_stdout_pipe(op->process));
    op_read_next(op);
    g_subprocess_wait_async(op->process, NULL, op_finished, op);
}

/* ---------- confirm + start package action ---------- */

typedef struct { GtkApplication *app; GtkWindow *parent; char *package; gboolean install; PackageManager pm; } PendingAction;

static void pending_free(PendingAction *p) { g_free(p->package); g_free(p); }

static void confirm_answer(GObject *src, GAsyncResult *res, gpointer data)
{
    PendingAction *p = data;
    GError *error = NULL;
    int choice = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(src), res, &error);
    g_clear_error(&error);
    if (choice == 1) {
        GPtrArray *argv = pm_command(p->pm, p->install, p->package);
        if (argv) {
            char *detail = g_strdup_printf("Package: %s\nPackage manager: %s\nAdministrator authentication may be requested.",
                                           p->package, pm_name(p->pm));
            run_logged(p->app, p->install ? "Installing application" : "Uninstalling application", detail, argv);
            g_free(detail);
            g_ptr_array_unref(argv);
        }
    }
    g_object_unref(src);
    pending_free(p);
}

static void package_clicked(GtkButton *b, gpointer data)
{
    GtkWindow *parent = GTK_WINDOW(data);
    PackageManager pm = detect_pm();
    const char *package = g_object_get_data(G_OBJECT(b), "lh-package");
    gboolean install = g_object_get_data(G_OBJECT(b), "lh-install") != NULL;
    GtkAlertDialog *dialog;
    PendingAction *p;
    char *title, *detail;
    const char *buttons[3];

    if (!package) return;
    if (pm == PM_NONE) {
        dialog = gtk_alert_dialog_new("No package manager found");
        gtk_alert_dialog_set_detail(dialog, lh4051_platform_is_windows()
            ? "winget is required to install applications. Install \"App Installer\" from the Microsoft Store."
            : "No supported package manager (PackageKit, pacman, apt, dnf, zypper) was found.");
        gtk_alert_dialog_show(dialog, parent);
        g_object_unref(dialog);
        return;
    }

    p = g_new0(PendingAction, 1);
    p->app = gtk_window_get_application(parent);
    p->parent = parent;
    p->package = g_strdup(package);
    p->install = install;
    p->pm = pm;

    title = g_strdup_printf("%s application?", install ? "Install" : "Uninstall");
    detail = g_strdup_printf("Package: %s\nPackage manager: %s\n\nThis may require administrator authentication.",
                             package, pm_name(pm));
    dialog = gtk_alert_dialog_new("%s", title);
    gtk_alert_dialog_set_detail(dialog, detail);
    buttons[0] = "Cancel"; buttons[1] = install ? "Install" : "Uninstall"; buttons[2] = NULL;
    gtk_alert_dialog_set_buttons(dialog, buttons);
    gtk_alert_dialog_set_cancel_button(dialog, 0);
    gtk_alert_dialog_set_default_button(dialog, 0);
    gtk_alert_dialog_choose(dialog, parent, NULL, confirm_answer, p);
    g_free(title); g_free(detail);
}

/* ---------- cards ---------- */

static void open_app(GtkButton *b, gpointer data)
{
    StoreApp *a = data;
    GError *error = NULL;
    (void)b;
    if (a->app && !g_app_info_launch(a->app, NULL, NULL, &error)) {
        g_warning("LH4051 App Store: %s", error ? error->message : "launch failed");
        g_clear_error(&error);
    }
}

static void about_app(GtkButton *b, gpointer data)
{
    StoreApp *a = data;
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", a->name ? a->name : "Application");
    char *detail = g_strdup_printf("%s\n\nIdentifier: %s\nPackage: %s\nHomepage: %s",
        a->summary ? a->summary : "No description available.", a->id ? a->id : "unknown",
        a->package ? a->package : "unknown", a->homepage ? a->homepage : "not provided");
    gtk_alert_dialog_set_detail(dialog, detail);
    gtk_alert_dialog_show(dialog, GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(b))));
    g_free(detail);
    g_object_unref(dialog);
}

static GtkWidget *make_card(StoreApp *a, GtkWindow *parent, gboolean installed)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    GtkWidget *icon_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *title = gtk_label_new(a->name ? a->name : "Application");
    GtkWidget *summary = gtk_label_new(a->summary ? a->summary : "");
    GtkWidget *image, *b;

    if (a->gicon) image = gtk_image_new_from_gicon(a->gicon);
    else if (a->icon && g_path_is_absolute(a->icon)) image = gtk_image_new_from_file(a->icon);
    else image = gtk_image_new_from_icon_name(a->icon && *a->icon ? a->icon : "application-x-executable");
    gtk_image_set_pixel_size(GTK_IMAGE(image), 48);

    gtk_widget_add_css_class(card, "lh-store-app-card");
    gtk_widget_add_css_class(icon_box, "lh-store-icon-box");
    gtk_widget_add_css_class(title, "lh-store-app-name");
    gtk_widget_add_css_class(summary, "lh-store-app-summary");
    gtk_widget_set_hexpand(body, TRUE);
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_label_set_xalign(GTK_LABEL(summary), 0);
    gtk_label_set_wrap(GTK_LABEL(summary), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(summary), 60);
    gtk_label_set_lines(GTK_LABEL(summary), 2);
    gtk_label_set_ellipsize(GTK_LABEL(summary), PANGO_ELLIPSIZE_END);

    gtk_box_append(GTK_BOX(icon_box), image);
    gtk_box_append(GTK_BOX(body), title);
    gtk_box_append(GTK_BOX(body), summary);

    b = gtk_button_new_with_label("About");
    g_signal_connect(b, "clicked", G_CALLBACK(about_app), a);
    gtk_box_append(GTK_BOX(actions), b);
    if (installed && a->app) {
        b = gtk_button_new_with_label("Open");
        g_signal_connect(b, "clicked", G_CALLBACK(open_app), a);
        gtk_box_append(GTK_BOX(actions), b);
    }
    if (a->package) {
        b = gtk_button_new_with_label(installed ? "Uninstall" : "Install");
        g_object_set_data_full(G_OBJECT(b), "lh-package", g_strdup(a->package), g_free);
        if (!installed) g_object_set_data(G_OBJECT(b), "lh-install", GINT_TO_POINTER(1));
        g_signal_connect(b, "clicked", G_CALLBACK(package_clicked), parent);
        gtk_box_append(GTK_BOX(actions), b);
    }
    gtk_box_append(GTK_BOX(body), actions);
    gtk_box_append(GTK_BOX(card), icon_box);
    gtk_box_append(GTK_BOX(card), body);
    g_object_set_data_full(G_OBJECT(card), "lh-store-app", a, (GDestroyNotify)store_app_free);
    return card;
}

/* ---------- AppStream search ---------- */

static StoreApp *parse_appstream_block(const char *block)
{
    StoreApp *a = g_new0(StoreApp, 1);
    char **lines = g_strsplit(block, "\n", -1);
    for (guint i = 0; lines[i]; i++) {
        char *line = lines[i], *colon = strchr(line, ':');
        char *value;
        if (!colon) continue;
        *colon = 0;
        value = g_strstrip(colon + 1);
        g_strstrip(line);
        if (g_strcmp0(line, "Identifier") == 0) {
            char *sp = strchr(value, ' '); /* "org.foo.Bar [desktop-application]" */
            a->id = sp ? g_strndup(value, (gsize)(sp - value)) : g_strdup(value);
        }
        else if (g_strcmp0(line, "Name") == 0) a->name = g_strdup(value);
        else if (g_strcmp0(line, "Summary") == 0) a->summary = g_strdup(value);
        else if (g_strcmp0(line, "Package") == 0) a->package = g_strdup(value);
        else if (g_strcmp0(line, "Homepage") == 0) a->homepage = g_strdup(value);
        else if (g_strcmp0(line, "Icon") == 0) { g_free(a->icon); a->icon = g_strdup(value); }
    }
    g_strfreev(lines);
    if (!a->name && !a->id) { store_app_free(a); return NULL; }
    return a;
}

typedef struct { GtkWidget *flow, *status; GtkWindow *window; guint generation; } SearchCtx;

static guint search_generation;

static void clear_flow(GtkWidget *flow)
{
    GtkWidget *c;
    while ((c = gtk_widget_get_first_child(flow)) != NULL) gtk_flow_box_remove(GTK_FLOW_BOX(flow), c);
}

static void search_done(GObject *src, GAsyncResult *res, gpointer data)
{
    SearchCtx *ctx = data;
    char *out = NULL, *err = NULL;
    GError *error = NULL;
    guint found = 0;
    gboolean ok = g_subprocess_communicate_utf8_finish(G_SUBPROCESS(src), res, &out, &err, &error);

    if (!ctx->flow || !ctx->status || ctx->generation != search_generation) {
        /* window closed or a newer search superseded this one */
    } else if (ok) {
        char **blocks = g_strsplit(out ? out : "", "\n---\n", -1);
        for (guint i = 0; blocks[i]; i++) {
            StoreApp *a = parse_appstream_block(blocks[i]);
            if (!a) continue;
            gtk_flow_box_insert(GTK_FLOW_BOX(ctx->flow), make_card(a, ctx->window, FALSE), -1);
            found++;
        }
        g_strfreev(blocks);
        {
            char *m = found ? g_strdup_printf("%u result%s", found, found == 1 ? "" : "s")
                            : g_strdup("No matching applications.");
            gtk_label_set_text(GTK_LABEL(ctx->status), m);
            g_free(m);
        }
    } else {
        gtk_label_set_text(GTK_LABEL(ctx->status), error ? error->message : "Search failed.");
    }
    g_clear_error(&error);
    g_free(out); g_free(err);
    if (ctx->flow) g_object_remove_weak_pointer(G_OBJECT(ctx->flow), (gpointer *)&ctx->flow);
    if (ctx->status) g_object_remove_weak_pointer(G_OBJECT(ctx->status), (gpointer *)&ctx->status);
    g_object_unref(src);
    g_free(ctx);
}

static void do_search(GtkWidget *entry, GtkWidget *flow, GtkWidget *status, GtkWindow *window)
{
    const char *query = gtk_editable_get_text(GTK_EDITABLE(entry));
    GSubprocess *proc;
    GError *error = NULL;
    SearchCtx *ctx;

    search_generation++;
    clear_flow(flow);
    if (!query || !*query) { gtk_label_set_text(GTK_LABEL(status), "Type a name to search."); return; }

    if (!g_find_program_in_path("appstreamcli")) {
        gtk_label_set_text(GTK_LABEL(status), lh4051_platform_is_windows()
            ? "Catalog search needs AppStream (not available on Windows). Use winget to find packages."
            : "Catalog search needs appstreamcli (package: appstream).");
        return;
    }
    proc = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &error,
                            "appstreamcli", "search", query, NULL);
    if (!proc) {
        gtk_label_set_text(GTK_LABEL(status), error ? error->message : "Could not run appstreamcli.");
        g_clear_error(&error);
        return;
    }
    ctx = g_new0(SearchCtx, 1);
    ctx->flow = flow; ctx->status = status; ctx->window = window;
    ctx->generation = ++search_generation;
    g_object_add_weak_pointer(G_OBJECT(flow), (gpointer *)&ctx->flow);
    g_object_add_weak_pointer(G_OBJECT(status), (gpointer *)&ctx->status);
    gtk_label_set_text(GTK_LABEL(status), "Searching…");
    g_subprocess_communicate_utf8_async(proc, NULL, NULL, search_done, ctx);
}

static void search_clicked(GtkButton *b, gpointer data)
{
    (void)data;
    do_search(g_object_get_data(G_OBJECT(b), "lh-entry"), g_object_get_data(G_OBJECT(b), "lh-results"),
              g_object_get_data(G_OBJECT(b), "lh-status"), GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(b))));
}

static void search_entry_activate(GtkSearchEntry *e, gpointer data)
{
    gtk_widget_activate(GTK_WIDGET(data));
    (void)e;
}

/* ---------- installed apps ---------- */

static void populate_installed(GtkWidget *flow, GtkWindow *parent)
{
    GList *apps = g_app_info_get_all();
    for (GList *l = apps; l; l = l->next) {
        GAppInfo *info = l->data;
        StoreApp *a;
        if (!g_app_info_should_show(info) || !g_app_info_get_display_name(info)) continue;
        a = g_new0(StoreApp, 1);
        a->id = g_strdup(g_app_info_get_id(info));
        a->name = g_strdup(g_app_info_get_display_name(info));
        a->summary = g_strdup(g_app_info_get_description(info));
        a->gicon = g_app_info_get_icon(info) ? g_object_ref(g_app_info_get_icon(info)) : NULL;
        a->app = g_object_ref(info);
        gtk_flow_box_insert(GTK_FLOW_BOX(flow), make_card(a, parent, TRUE), -1);
    }
    g_list_free_full(apps, g_object_unref);
}

static GtkWindow *existing; /* single instance */

static void on_store_destroy(GtkWidget *w, gpointer d)
{
    (void)w; (void)d;
    existing = NULL;
}

void lh4051_appstore_open(GtkApplication *app)
{
    if (existing) { gtk_window_present(existing); return; }

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *entry = gtk_search_entry_new();
    GtkWidget *search = gtk_button_new_with_label("Search");
    GtkWidget *status = gtk_label_new("Search the AppStream catalog or manage installed applications.");
    GtkWidget *results = gtk_flow_box_new(), *installed = gtk_flow_box_new();
    GtkWidget *rscroll = gtk_scrolled_window_new(), *iscroll = gtk_scrolled_window_new();
    GtkWidget *rlabel = gtk_label_new("Catalog results"), *ilabel = gtk_label_new("Installed applications");
    GtkWidget *title = gtk_label_new("LH4051 App Store");
    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    gtk_widget_add_css_class(root, "lh-store");
    gtk_widget_add_css_class(header, "lh-store-header");
    gtk_widget_add_css_class(title, "lh-store-title");
    gtk_widget_add_css_class(status, "lh-store-status");
    gtk_widget_add_css_class(rlabel, "lh-store-section");
    gtk_widget_add_css_class(ilabel, "lh-store-section");
    gtk_widget_add_css_class(entry, "lh-store-search");
    gtk_widget_add_css_class(search, "lh-store-search-button");
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_label_set_xalign(GTK_LABEL(status), 0);
    gtk_label_set_xalign(GTK_LABEL(rlabel), 0);
    gtk_label_set_xalign(GTK_LABEL(ilabel), 0);

    gtk_widget_set_hexpand(entry, TRUE);
    gtk_box_append(GTK_BOX(row), entry);
    gtk_box_append(GTK_BOX(row), search);
    gtk_box_append(GTK_BOX(header), title);
    gtk_box_append(GTK_BOX(header), row);
    gtk_box_append(GTK_BOX(header), status);
    gtk_box_append(GTK_BOX(root), header);

    gtk_widget_set_margin_start(body, 12);
    gtk_widget_set_margin_end(body, 12);
    gtk_widget_set_margin_top(body, 8);
    gtk_widget_set_margin_bottom(body, 8);
    gtk_widget_set_vexpand(body, TRUE);
    gtk_box_append(GTK_BOX(body), rlabel);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(results), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(results), 2);
    gtk_widget_set_vexpand(rscroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rscroll), results);
    gtk_box_append(GTK_BOX(body), rscroll);
    gtk_box_append(GTK_BOX(body), ilabel);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(installed), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(installed), 2);
    gtk_widget_set_vexpand(iscroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(iscroll), installed);
    gtk_box_append(GTK_BOX(body), iscroll);
    gtk_box_append(GTK_BOX(root), body);

    g_object_set_data(G_OBJECT(search), "lh-entry", entry);
    g_object_set_data(G_OBJECT(search), "lh-results", results);
    g_object_set_data(G_OBJECT(search), "lh-status", status);
    g_signal_connect(search, "clicked", G_CALLBACK(search_clicked), NULL);
    g_signal_connect(entry, "activate", G_CALLBACK(search_entry_activate), search);

    existing = lh4051_wm_create_window(app, "LH4051 App Store", 980, 720, root);
    g_signal_connect(existing, "destroy", G_CALLBACK(on_store_destroy), NULL);
    populate_installed(installed, existing);
    gtk_window_present(existing);
}
