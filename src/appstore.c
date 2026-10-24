#include "longhorn.h"
#include "LH4051-WM/wm.h"
#include <string.h>
#ifndef G_OS_WIN32
#include <unistd.h>
#endif

#define MAX_RESULTS 200

/* ======================================================================
 * Package managers
 *
 * One table drives install/remove/search for every supported package manager.
 * Templates are split on spaces; "%s" is replaced by the package / query.
 * ====================================================================== */

enum { OSF_LINUX = 1, OSF_MACOS = 2, OSF_BSD = 4, OSF_WINDOWS = 8 };

typedef enum {
    SEARCH_NONE,
    SEARCH_APPSTREAM, /* handled separately (appstreamcli) */
    SEARCH_DASH,      /* "name - summary" */
    SEARCH_NAMES,     /* one package name per line */
    SEARCH_PIPE,      /* "name|version" */
    SEARCH_FLATPAK,   /* tab separated: application, name, description */
    SEARCH_PORT,      /* tab separated: name, version, categories, description */
    SEARCH_WINGET     /* column table */
} SearchKind;

typedef struct {
    const char *label;
    const char *binary;
    unsigned os;       /* OSF_* mask */
    gboolean root;     /* needs privilege escalation on Unix */
    const char *install;
    const char *remove;
    const char *search;
    SearchKind kind;
} PackageManager;

static const PackageManager PMS[] = {
    /* Linux */
    {"PackageKit", "pkcon", OSF_LINUX, FALSE, "pkcon -y install %s", "pkcon -y remove %s", NULL, SEARCH_NONE},
    {"pacman", "pacman", OSF_LINUX, TRUE, "pacman -S --noconfirm %s", "pacman -R --noconfirm %s", NULL, SEARCH_NONE},
    {"APT", "apt-get", OSF_LINUX, TRUE, "apt-get install -y %s", "apt-get remove -y %s", "apt-cache search %s", SEARCH_DASH},
    {"DNF", "dnf", OSF_LINUX, TRUE, "dnf install -y %s", "dnf remove -y %s", NULL, SEARCH_NONE},
    {"zypper", "zypper", OSF_LINUX, TRUE, "zypper --non-interactive install %s", "zypper --non-interactive remove %s", NULL, SEARCH_NONE},
    {"apk", "apk", OSF_LINUX, TRUE, "apk add %s", "apk del %s", NULL, SEARCH_NONE},
    {"xbps", "xbps-install", OSF_LINUX, TRUE, "xbps-install -y %s", "xbps-remove -y %s", NULL, SEARCH_NONE},
    {"Portage", "emerge", OSF_LINUX, TRUE, "emerge %s", "emerge --unmerge %s", NULL, SEARCH_NONE},
    {"Snap", "snap", OSF_LINUX, TRUE, "snap install %s", "snap remove %s", NULL, SEARCH_NONE},
    {"Flatpak", "flatpak", OSF_LINUX, FALSE, "flatpak install -y %s", "flatpak uninstall -y %s",
     "flatpak search --columns=application,name,description %s", SEARCH_FLATPAK},
    {"Nix", "nix", OSF_LINUX | OSF_MACOS, FALSE, "nix profile install nixpkgs#%s", "nix profile remove %s", NULL, SEARCH_NONE},
    /* macOS (Homebrew also works on Linux) */
    {"Homebrew", "brew", OSF_MACOS | OSF_LINUX, FALSE, "brew install %s", "brew uninstall %s", "brew search %s", SEARCH_NAMES},
    {"MacPorts", "port", OSF_MACOS, TRUE, "port install %s", "port uninstall %s", "port search --name --line %s", SEARCH_PORT},
    /* BSD */
    {"pkg", "pkg", OSF_BSD, TRUE, "pkg install -y %s", "pkg delete -y %s", "pkg search -q %s", SEARCH_NAMES},
    {"pkgin", "pkgin", OSF_BSD, TRUE, "pkgin -y install %s", "pkgin -y remove %s", NULL, SEARCH_NONE},
    {"pkg_add", "pkg_add", OSF_BSD, TRUE, "pkg_add %s", "pkg_delete %s", "pkg_info -Q %s", SEARCH_NAMES},
    /* Windows */
    {"winget", "winget", OSF_WINDOWS, FALSE,
     "winget install --id %s -e --accept-source-agreements --accept-package-agreements",
     "winget uninstall --id %s -e", "winget search %s --accept-source-agreements", SEARCH_WINGET},
    {"Chocolatey", "choco", OSF_WINDOWS, FALSE, "choco install %s -y", "choco uninstall %s -y", "choco search %s -r", SEARCH_PIPE},
    {"Scoop", "scoop", OSF_WINDOWS, FALSE, "scoop install %s", "scoop uninstall %s", NULL, SEARCH_NONE},
};

static unsigned current_os(void)
{
#if defined(G_OS_WIN32)
    return OSF_WINDOWS;
#elif defined(LH_OS_MACOS)
    return OSF_MACOS;
#elif defined(LH_OS_BSD)
    return OSF_BSD;
#else
    return OSF_LINUX;
#endif
}

static GPtrArray *detect_pms(void)
{
    GPtrArray *found = g_ptr_array_new();
    for (guint i = 0; i < G_N_ELEMENTS(PMS); i++) {
        char *path;
        if (!(PMS[i].os & current_os())) continue;
        path = g_find_program_in_path(PMS[i].binary);
        if (path) { g_ptr_array_add(found, (gpointer)&PMS[i]); g_free(path); }
    }
    return found;
}

/* First available of pkexec / doas / sudo, or NULL (also NULL when already root). */
static const char *escalator(void)
{
#ifdef G_OS_WIN32
    return NULL;
#else
    static const char *names[] = {"pkexec", "doas", "sudo", NULL};
    if (geteuid() == 0) return NULL;
    for (int i = 0; names[i]; i++) {
        char *p = g_find_program_in_path(names[i]);
        if (p) { g_free(p); return names[i]; }
    }
    return NULL;
#endif
}

static gboolean needs_escalator_but_missing(const PackageManager *pm)
{
#ifdef G_OS_WIN32
    (void)pm;
    return FALSE;
#else
    if (!pm->root || geteuid() == 0) return FALSE;
    return escalator() == NULL;
#endif
}

/* Builds a NULL-terminated argv from a template (+ escalator if required). */
static GPtrArray *build_argv(const PackageManager *pm, const char *tmpl, const char *arg)
{
    GPtrArray *a = g_ptr_array_new_with_free_func(g_free);
    char **parts = g_strsplit(tmpl, " ", -1);
    const char *esc = pm->root ? escalator() : NULL;
    if (esc) g_ptr_array_add(a, g_strdup(esc));
    for (int i = 0; parts[i]; i++) {
        if (!*parts[i]) continue;
        if (g_strcmp0(parts[i], "%s") == 0) g_ptr_array_add(a, g_strdup(arg));
        else if (strstr(parts[i], "%s")) {
            char **pieces = g_strsplit(parts[i], "%s", 2); /* e.g. nixpkgs#%s */
            g_ptr_array_add(a, g_strconcat(pieces[0], arg, pieces[1] ? pieces[1] : "", NULL));
            g_strfreev(pieces);
        } else g_ptr_array_add(a, g_strdup(parts[i]));
    }
    g_strfreev(parts);
    g_ptr_array_add(a, NULL);
    return a;
}

/* ---------- store entries ---------- */

typedef struct {
    char *id, *name, *summary, *package, *homepage, *icon;
    GIcon *gicon;
    const LHApp *app; /* installed apps; owned by the window's app list */
} StoreApp;

static void store_app_free(StoreApp *a)
{
    if (!a) return;
    g_free(a->id); g_free(a->name); g_free(a->summary);
    g_free(a->package); g_free(a->homepage); g_free(a->icon);
    g_clear_object(&a->gicon);
    g_free(a);
}

static GPtrArray *detected;      /* const PackageManager*, valid while window exists */
static GtkWidget *pm_dropdown;

static const PackageManager *current_pm(void)
{
    guint i;
    if (!detected || !detected->len || !pm_dropdown) return NULL;
    i = gtk_drop_down_get_selected(GTK_DROP_DOWN(pm_dropdown));
    return i < detected->len ? g_ptr_array_index(detected, i) : NULL;
}

/* ======================================================================
 * Live operation window
 * ====================================================================== */

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

/* ======================================================================
 * Confirm + run package action
 * ====================================================================== */

typedef struct { GtkApplication *app; char *package; gboolean install; const PackageManager *pm; } PendingAction;

static void pending_free(PendingAction *p) { g_free(p->package); g_free(p); }

static void confirm_answer(GObject *src, GAsyncResult *res, gpointer data)
{
    PendingAction *p = data;
    GError *error = NULL;
    int choice = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(src), res, &error);
    g_clear_error(&error);
    if (choice == 1) {
        GPtrArray *argv = build_argv(p->pm, p->install ? p->pm->install : p->pm->remove, p->package);
        char *detail = g_strdup_printf("Package: %s\nPackage manager: %s%s", p->package, p->pm->label,
            g_strcmp0(p->pm->binary, "choco") == 0 ? "\nChocolatey needs an elevated (administrator) session." : "");
        run_logged(p->app, p->install ? "Installing application" : "Uninstalling application", detail, argv);
        g_free(detail);
        g_ptr_array_unref(argv);
    }
    g_object_unref(src);
    pending_free(p);
}

static void show_alert(GtkWindow *parent, const char *title, const char *detail)
{
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", title);
    gtk_alert_dialog_set_detail(dialog, detail);
    gtk_alert_dialog_show(dialog, parent);
    g_object_unref(dialog);
}

static void package_clicked(GtkButton *b, gpointer data)
{
    GtkWindow *parent = GTK_WINDOW(data);
    const PackageManager *pm = current_pm();
    const char *package = g_object_get_data(G_OBJECT(b), "lh-package");
    gboolean install = g_object_get_data(G_OBJECT(b), "lh-install") != NULL;
    GtkAlertDialog *dialog;
    PendingAction *p;
    char *title, *detail;
    const char *buttons[3];

    if (!package) return;
    if (!pm) {
        show_alert(parent, "No package manager found",
                   lh4051_platform_is_windows()
                       ? "Install winget (App Installer from the Microsoft Store), Chocolatey or Scoop."
                       : "No supported package manager was found on this system.");
        return;
    }
    if (needs_escalator_but_missing(pm)) {
        show_alert(parent, "Administrator rights required",
                   "Install pkexec (polkit), doas or sudo, or start LH4051-DE as root, to use this package manager.");
        return;
    }

    p = g_new0(PendingAction, 1);
    p->app = gtk_window_get_application(parent);
    p->package = g_strdup(package);
    p->install = install;
    p->pm = pm;

    title = g_strdup_printf("%s application?", install ? "Install" : "Uninstall");
    detail = g_strdup_printf("Package: %s\nPackage manager: %s\n\nThis may require administrator authentication.",
                             package, pm->label);
    dialog = gtk_alert_dialog_new("%s", title);
    gtk_alert_dialog_set_detail(dialog, detail);
    buttons[0] = "Cancel"; buttons[1] = install ? "Install" : "Uninstall"; buttons[2] = NULL;
    gtk_alert_dialog_set_buttons(dialog, buttons);
    gtk_alert_dialog_set_cancel_button(dialog, 0);
    gtk_alert_dialog_set_default_button(dialog, 0);
    gtk_alert_dialog_choose(dialog, parent, NULL, confirm_answer, p);
    g_free(title); g_free(detail);
}

/* ======================================================================
 * Cards
 * ====================================================================== */

static void open_app(GtkButton *b, gpointer data)
{
    StoreApp *a = data;
    GError *error = NULL;
    (void)b;
    if (a->app && !lh4051_app_launch(a->app, &error)) {
        g_warning("LH4051 App Store: %s", error ? error->message : "launch failed");
        g_clear_error(&error);
    }
}

static void about_app(GtkButton *b, gpointer data)
{
    StoreApp *a = data;
    char *detail = g_strdup_printf("%s\n\nIdentifier: %s\nPackage: %s\nHomepage: %s",
        a->summary ? a->summary : "No description available.", a->id ? a->id : "unknown",
        a->package ? a->package : "unknown", a->homepage ? a->homepage : "not provided");
    show_alert(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(b))), a->name ? a->name : "Application", detail);
    g_free(detail);
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

/* ======================================================================
 * Search result parsers
 * ====================================================================== */

static StoreApp *store_entry(const char *package, const char *name, const char *summary)
{
    StoreApp *a = g_new0(StoreApp, 1);
    a->package = g_strdup(package);
    a->id = g_strdup(package);
    a->name = g_strdup(name && *name ? name : package);
    a->summary = (summary && *summary) ? g_strdup(summary) : NULL;
    return a;
}

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

/* winget prints a space-aligned table; find the Id/Version columns from the header. */
static void parse_winget(const char *out, GPtrArray *res)
{
    char **lines = g_strsplit(out, "\n", -1);
    gboolean in_rows = FALSE;
    gsize id_col = 0, ver_col = 0;
    const char *header = NULL;

    for (guint i = 0; lines[i] && res->len < MAX_RESULTS; i++) {
        char *line = g_strchomp(lines[i]);
        char *carriage = strrchr(line, '\r');
        if (carriage) line = carriage + 1;
        if (!in_rows) {
            if (g_str_has_prefix(line, "Name")) header = line;
            if (header && g_str_has_prefix(line, "---")) {
                const char *id = strstr(header, "Id"), *ver = strstr(header, "Version");
                if (!id) break;
                id_col = (gsize)(id - header);
                ver_col = ver ? (gsize)(ver - header) : strlen(header);
                in_rows = TRUE;
            }
            continue;
        }
        if (strlen(line) <= id_col) continue;
        {
            char *name = g_strstrip(g_strndup(line, id_col));
            char *id = g_strstrip(g_strndup(line + id_col, (strlen(line) > ver_col ? ver_col : strlen(line)) - id_col));
            char *ver = strlen(line) > ver_col ? g_strstrip(g_strdup(line + ver_col)) : g_strdup("");
            char *summary = *ver ? g_strdup_printf("Version %s", ver) : NULL;
            if (*id) g_ptr_array_add(res, store_entry(id, name, summary));
            g_free(name); g_free(id); g_free(ver); g_free(summary);
        }
    }
    g_strfreev(lines);
}

static GPtrArray *parse_results(SearchKind kind, const char *out)
{
    GPtrArray *res = g_ptr_array_new_with_free_func((GDestroyNotify)store_app_free);
    char **lines;

    if (!out) return res;
    if (kind == SEARCH_APPSTREAM) {
        char **blocks = g_strsplit(out, "\n---\n", -1);
        for (guint i = 0; blocks[i] && res->len < MAX_RESULTS; i++) {
            StoreApp *a = parse_appstream_block(blocks[i]);
            if (a) g_ptr_array_add(res, a);
        }
        g_strfreev(blocks);
        return res;
    }
    if (kind == SEARCH_WINGET) { parse_winget(out, res); return res; }

    lines = g_strsplit(out, "\n", -1);
    for (guint i = 0; lines[i] && res->len < MAX_RESULTS; i++) {
        char *line = g_strstrip(lines[i]);
        if (!*line || *line == '=' || *line == '#') continue;
        switch (kind) {
        case SEARCH_DASH: {
            char *sep = strstr(line, " - ");
            if (sep) { char *name = g_strndup(line, (gsize)(sep - line));
                       g_ptr_array_add(res, store_entry(name, name, sep + 3)); g_free(name); }
            break;
        }
        case SEARCH_NAMES: {
            if (!strchr(line, ' ')) g_ptr_array_add(res, store_entry(line, line, NULL));
            break;
        }
        case SEARCH_PIPE: {
            char **c = g_strsplit(line, "|", 2);
            if (c[0] && *c[0]) {
                char *summary = c[1] ? g_strdup_printf("Version %s", c[1]) : NULL;
                g_ptr_array_add(res, store_entry(c[0], c[0], summary));
                g_free(summary);
            }
            g_strfreev(c);
            break;
        }
        case SEARCH_FLATPAK: {
            char **c = g_strsplit(line, "\t", 3);
            if (c[0] && c[1] && *c[0]) g_ptr_array_add(res, store_entry(c[0], c[1], c[2]));
            g_strfreev(c);
            break;
        }
        case SEARCH_PORT: {
            char **c = g_strsplit(line, "\t", 5);
            if (c[0] && *c[0]) g_ptr_array_add(res, store_entry(c[0], c[0], c[1] && c[2] && c[3] ? c[3] : NULL));
            g_strfreev(c);
            break;
        }
        default: break;
        }
    }
    g_strfreev(lines);
    return res;
}

/* ======================================================================
 * Search
 * ====================================================================== */

typedef struct { GtkWidget *flow, *status; GtkWindow *window; guint generation; SearchKind kind; } SearchCtx;

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
    gboolean ok = g_subprocess_communicate_utf8_finish(G_SUBPROCESS(src), res, &out, &err, &error);

    if (!ctx->flow || !ctx->status || ctx->generation != search_generation) {
        /* window closed or a newer search superseded this one */
    } else if (ok) {
        GPtrArray *results = parse_results(ctx->kind, out);
        guint n = results->len;
        for (guint i = 0; i < n; i++)
            gtk_flow_box_insert(GTK_FLOW_BOX(ctx->flow),
                                make_card(g_ptr_array_steal_index(results, 0), ctx->window, FALSE), -1);
        g_ptr_array_unref(results);
        {
            char *m = n ? g_strdup_printf("%u result%s", n, n == 1 ? "" : "s") : g_strdup("No matching applications.");
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
    const PackageManager *pm = current_pm();
    GPtrArray *argv = NULL;
    SearchKind kind = SEARCH_NONE;
    GSubprocess *proc;
    GError *error = NULL;
    SearchCtx *ctx;

    search_generation++;
    clear_flow(flow);
    if (!query || !*query) { gtk_label_set_text(GTK_LABEL(status), "Type a name to search."); return; }

    if (pm && pm->search && pm->kind != SEARCH_NONE) {
        argv = build_argv(pm, pm->search, query);
        kind = pm->kind;
    } else if (!lh4051_platform_is_windows() && g_find_program_in_path("appstreamcli")) {
        argv = g_ptr_array_new_with_free_func(g_free);
        g_ptr_array_add(argv, g_strdup("appstreamcli"));
        g_ptr_array_add(argv, g_strdup("search"));
        g_ptr_array_add(argv, g_strdup(query));
        g_ptr_array_add(argv, NULL);
        kind = SEARCH_APPSTREAM;
    } else {
        char *m = g_strdup_printf("%s cannot search from here. Pick another source, or install appstreamcli.",
                                  pm ? pm->label : "No package manager");
        gtk_label_set_text(GTK_LABEL(status), m);
        g_free(m);
        return;
    }

    proc = g_subprocess_newv((const char *const *)argv->pdata,
                             G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, &error);
    g_ptr_array_unref(argv);
    if (!proc) {
        gtk_label_set_text(GTK_LABEL(status), error ? error->message : "Could not start the search.");
        g_clear_error(&error);
        return;
    }
    ctx = g_new0(SearchCtx, 1);
    ctx->flow = flow; ctx->status = status; ctx->window = window; ctx->kind = kind;
    ctx->generation = search_generation;
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
    (void)e;
    gtk_widget_activate(GTK_WIDGET(data));
}

/* ======================================================================
 * Window
 * ====================================================================== */

static void populate_installed(GtkWidget *flow, GtkWindow *parent, GPtrArray *apps)
{
    for (guint i = 0; i < apps->len; i++) {
        const LHApp *app = g_ptr_array_index(apps, i);
        StoreApp *a = g_new0(StoreApp, 1);
        a->id = g_strdup(app->id);
        a->name = g_strdup(app->name);
        a->summary = g_strdup(app->summary);
        a->gicon = app->icon ? g_object_ref(app->icon) : NULL;
        a->app = app;
        gtk_flow_box_insert(GTK_FLOW_BOX(flow), make_card(a, parent, TRUE), -1);
    }
}

static GtkWindow *existing; /* single instance */

static void on_store_destroy(GtkWidget *w, gpointer d)
{
    (void)w; (void)d;
    existing = NULL;
    pm_dropdown = NULL;
    g_clear_pointer(&detected, g_ptr_array_unref);
}

void lh4051_appstore_open(GtkApplication *app)
{
    GtkWidget *root, *header, *row, *entry, *search, *status, *results, *installed;
    GtkWidget *rscroll, *iscroll, *rlabel, *ilabel, *title, *body, *pm_row, *pm_label;
    GPtrArray *apps;
    char *source_text;

    if (existing) { gtk_window_present(existing); return; }

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    pm_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    entry = gtk_search_entry_new();
    search = gtk_button_new_with_label("Search");
    status = gtk_label_new("");
    results = gtk_flow_box_new();
    installed = gtk_flow_box_new();
    rscroll = gtk_scrolled_window_new();
    iscroll = gtk_scrolled_window_new();
    rlabel = gtk_label_new("Search results");
    ilabel = gtk_label_new("Installed applications");
    title = gtk_label_new("LH4051 App Store");
    body = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    pm_label = gtk_label_new("Package manager:");

    detected = detect_pms();
    {
        GPtrArray *names = g_ptr_array_new();
        for (guint i = 0; i < detected->len; i++)
            g_ptr_array_add(names, (gpointer)((const PackageManager *)g_ptr_array_index(detected, i))->label);
        if (!names->len) g_ptr_array_add(names, (gpointer)"none found");
        g_ptr_array_add(names, NULL);
        pm_dropdown = gtk_drop_down_new_from_strings((const char *const *)names->pdata);
        g_ptr_array_unref(names);
    }
    gtk_widget_set_sensitive(pm_dropdown, detected->len > 0);

    source_text = g_strdup_printf("Manage applications on %s. %u package manager%s detected.",
                                  lh4051_platform_name(), detected->len, detected->len == 1 ? "" : "s");
    gtk_label_set_text(GTK_LABEL(status), source_text);
    g_free(source_text);

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
    gtk_box_append(GTK_BOX(pm_row), pm_label);
    gtk_box_append(GTK_BOX(pm_row), pm_dropdown);
    gtk_box_append(GTK_BOX(header), title);
    gtk_box_append(GTK_BOX(header), row);
    gtk_box_append(GTK_BOX(header), pm_row);
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

    existing = lh4051_wm_create_window(app, "LH4051 App Store", 980, 760, root);
    g_signal_connect(existing, "destroy", G_CALLBACK(on_store_destroy), NULL);
    apps = lh4051_apps_list();
    g_object_set_data_full(G_OBJECT(existing), "lh-apps", apps, (GDestroyNotify)g_ptr_array_unref);
    populate_installed(installed, existing, apps);
    gtk_window_present(existing);
}
