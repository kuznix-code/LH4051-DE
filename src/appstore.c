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
    SEARCH_WINGET,    /* column table: Name, Id, Version, ... */
    SEARCH_SCOOP,     /* column table: Name, Version, Source, ... */
    SEARCH_SNAP,      /* column table: Name, ..., Summary */
    SEARCH_PACMAN,    /* "repo/name version" + indented summary line */
    SEARCH_DNF,       /* "name.arch : summary" */
    SEARCH_ZYPPER,    /* "S | Name | Summary | Type" */
    SEARCH_APK,       /* "name-1.2-r0 - summary" */
    SEARCH_XBPS,      /* "[-] name-1.2_1  summary" */
    SEARCH_PKGIN,     /* "name-1.2nb1  summary" */
    SEARCH_EMERGE,    /* "*  category/name" + "Description:" line */
    SEARCH_NIX,       /* "* legacyPackages.<system>.name (version)" + indented summary */
    SEARCH_PKGKIT     /* pkcon --plain: "Available  name-1.2-1.arch (repo)  summary" */
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
    /* Linux. PackageKit goes first: when pkcon exists it is the distribution-neutral choice,
     * and the native package managers below are the fallback when it is missing or fails. */
    {"PackageKit", "pkcon", OSF_LINUX, FALSE, "pkcon -y install %s", "pkcon -y remove %s", "pkcon -p search name %s", SEARCH_PKGKIT},
    {"pacman", "pacman", OSF_LINUX, TRUE, "pacman -S --noconfirm %s", "pacman -R --noconfirm %s", "pacman -Ss %s", SEARCH_PACMAN},
    {"APT", "apt-get", OSF_LINUX, TRUE, "apt-get install -y %s", "apt-get remove -y %s", "apt-cache search %s", SEARCH_DASH},
    {"DNF", "dnf", OSF_LINUX, TRUE, "dnf install -y %s", "dnf remove -y %s", "dnf -q search %s", SEARCH_DNF},
    {"zypper", "zypper", OSF_LINUX, TRUE, "zypper --non-interactive install %s", "zypper --non-interactive remove %s",
     "zypper --non-interactive search -t package %s", SEARCH_ZYPPER},
    {"apk", "apk", OSF_LINUX, TRUE, "apk add %s", "apk del %s", "apk search -v %s", SEARCH_APK},
    {"xbps", "xbps-install", OSF_LINUX, TRUE, "xbps-install -y %s", "xbps-remove -y %s", "xbps-query -Rs %s", SEARCH_XBPS},
    {"Portage", "emerge", OSF_LINUX, TRUE, "emerge %s", "emerge --unmerge %s", "emerge --color n --search %s", SEARCH_EMERGE},
    {"Snap", "snap", OSF_LINUX, TRUE, "snap install %s", "snap remove %s", "snap find %s", SEARCH_SNAP},
    {"Flatpak", "flatpak", OSF_LINUX, FALSE, "flatpak install -y %s", "flatpak uninstall -y %s",
     "flatpak search --columns=application,name,description %s", SEARCH_FLATPAK},
    {"Nix", "nix", OSF_LINUX | OSF_MACOS, FALSE, "nix profile install nixpkgs#%s", "nix profile remove %s",
     "nix --extra-experimental-features nix-command --extra-experimental-features flakes search nixpkgs %s", SEARCH_NIX},
    /* macOS (Homebrew also works on Linux) */
    {"Homebrew", "brew", OSF_MACOS | OSF_LINUX, FALSE, "brew install %s", "brew uninstall %s", "brew search %s", SEARCH_NAMES},
    {"MacPorts", "port", OSF_MACOS, TRUE, "port install %s", "port uninstall %s", "port search --name --line %s", SEARCH_PORT},
    /* BSD */
    {"pkg", "pkg", OSF_BSD, TRUE, "pkg install -y %s", "pkg delete -y %s", "pkg search -q %s", SEARCH_NAMES},
    {"pkgin", "pkgin", OSF_BSD, TRUE, "pkgin -y install %s", "pkgin -y remove %s", "pkgin search %s", SEARCH_PKGIN},
    {"pkg_add", "pkg_add", OSF_BSD, TRUE, "pkg_add %s", "pkg_delete %s", "pkg_info -Q %s", SEARCH_NAMES},
    /* Windows */
    {"winget", "winget", OSF_WINDOWS, FALSE,
     "winget install --id %s -e --accept-source-agreements --accept-package-agreements",
     "winget uninstall --id %s -e", "winget search %s --source winget --accept-source-agreements", SEARCH_WINGET},
    {"Chocolatey", "choco", OSF_WINDOWS, FALSE, "choco install %s -y", "choco uninstall %s -y", "choco search %s -r", SEARCH_PIPE},
    {"Scoop", "scoop", OSF_WINDOWS, FALSE, "scoop install %s", "scoop uninstall %s", "scoop search %s", SEARCH_SCOOP},
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

/* Builds a NULL-terminated argv from a template. Install/remove commands get the privilege
 * escalator when the package manager needs one; read-only searches (escalate == FALSE) never do. */
static GPtrArray *build_argv(const PackageManager *pm, const char *tmpl, const char *arg, gboolean escalate)
{
    GPtrArray *a = g_ptr_array_new_with_free_func(g_free);
    char **parts = g_strsplit(tmpl, " ", -1);
    const char *esc = (escalate && pm->root) ? escalator() : NULL;
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
 * Process launching
 * ====================================================================== */

#ifdef G_OS_WIN32
/* .cmd/.bat shims (Scoop) cannot be started directly; they run through cmd.exe. Only used when every
 * argument is made of characters cmd.exe treats literally, so nothing from a search result (or a
 * package name) can inject a command. */
static GPtrArray *wrap_in_cmd(GPtrArray *argv)
{
    GPtrArray *w = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(w, g_strdup("cmd.exe"));
    g_ptr_array_add(w, g_strdup("/d"));
    g_ptr_array_add(w, g_strdup("/c"));
    for (guint i = 0; i < argv->len && g_ptr_array_index(argv, i); i++) {
        const char *a = g_ptr_array_index(argv, i);
        if (!*a) goto bad;
        for (const char *c = a; *c; c++)
            if (!g_ascii_isalnum(*c) && !strchr("._+-:/@#=,\\", *c)) goto bad;
        g_ptr_array_add(w, g_strdup(a));
    }
    g_ptr_array_add(w, NULL);
    return w;
bad:
    g_ptr_array_unref(w);
    return NULL;
}

/* Resolves argv[0] (winget.exe may be an App Execution Alias in %LOCALAPPDATA%\Microsoft\WindowsApps)
 * to a full path; scripts go through cmd.exe. Returns a new NULL-terminated argv, or NULL with *error set. */
static GPtrArray *win_prepare(GPtrArray *argv, GError **error)
{
    const char *prog = argv->len ? g_ptr_array_index(argv, 0) : NULL;
    char *path = prog ? g_find_program_in_path(prog) : NULL;
    GPtrArray *out = NULL;

    if (!prog) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "No command given.");
        return NULL;
    }
    if (!path) {
        /* GLib cannot always see App Execution Aliases (winget); CreateProcess searches PATH itself. */
        out = g_ptr_array_new_with_free_func(g_free);
        for (guint i = 0; i < argv->len && g_ptr_array_index(argv, i); i++)
            g_ptr_array_add(out, g_strdup(g_ptr_array_index(argv, i)));
        g_ptr_array_add(out, NULL);
        return out;
    }
    {
        char *lower = g_ascii_strdown(path, -1);
        gboolean is_exe = g_str_has_suffix(lower, ".exe");
        g_free(lower);
        if (is_exe) {
            out = g_ptr_array_new_with_free_func(g_free);
            g_ptr_array_add(out, path);
            path = NULL;
            for (guint i = 1; i < argv->len && g_ptr_array_index(argv, i); i++)
                g_ptr_array_add(out, g_strdup(g_ptr_array_index(argv, i)));
            g_ptr_array_add(out, NULL);
        } else {
            out = wrap_in_cmd(argv);
            if (!out)
                g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                            "\"%s\" is a script; its arguments contain characters that are not safe to pass through cmd.exe.", prog);
        }
    }
    g_free(path);
    return out;
}
#else
/* Starts a package-manager process (argv is NULL-terminated). */
static GSubprocess *spawn_pm(GPtrArray *argv, GSubprocessFlags flags, GError **error)
{
    return g_subprocess_newv((const char *const *)argv->pdata, flags, error);
}
#endif

/* Command output as valid UTF-8. Windows tools may answer in the console code page. */
static char *output_to_utf8(GBytes *bytes)
{
    gsize len = 0;
    const char *data = bytes ? g_bytes_get_data(bytes, &len) : NULL;
    char *text;
    if (!data || !len) return g_strdup("");
    text = g_strndup(data, len);
    if (g_utf8_validate(text, -1, NULL)) return text;
    {
        char *converted = g_locale_to_utf8(text, -1, NULL, NULL, NULL);
        if (converted) { g_free(text); return converted; }
    }
    {
        char *valid = g_utf8_make_valid(text, -1);
        g_free(text);
        return valid;
    }
}

/* ======================================================================
 * Live operation window
 * ====================================================================== */

typedef struct {
    GtkWidget *output, *status, *close;
    GtkWindow *window;
    GSubprocess *process; /* unix */
    GPid pid;             /* Windows process handle */
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

static void op_complete(Operation *op, int status)
{
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

#ifdef G_OS_WIN32
static void op_child_exited(GPid pid, gint status, gpointer data)
{
    g_spawn_close_pid(pid);
    op_complete(data, status);
}
#else
static void op_finished(GObject *src, GAsyncResult *res, gpointer data)
{
    GError *error = NULL;
    gboolean ok = g_subprocess_wait_finish(G_SUBPROCESS(src), res, &error);
    int status = (ok && g_subprocess_get_if_exited(G_SUBPROCESS(src)))
                     ? g_subprocess_get_exit_status(G_SUBPROCESS(src)) : -1;
    g_clear_error(&error);
    op_complete(data, status);
}
#endif

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

#ifdef G_OS_WIN32
    {
        GPtrArray *prepared = win_prepare(argv, &error);
        GInputStream *in = prepared ? lh4051_win_spawn((const char *const *)prepared->pdata, TRUE, &op->pid, &error) : NULL;
        if (prepared) g_ptr_array_unref(prepared);
        if (!in) {
            op_append(op, error ? error->message : "Could not start the command.");
            gtk_label_set_text(GTK_LABEL(op->status), "Could not start the package manager");
            gtk_widget_add_css_class(op->status, "lh-operation-failed");
            g_clear_error(&error);
            return; /* window owns op; released in op_destroyed */
        }
        op->running = TRUE;
        op->refs++; /* child watch */
        gtk_widget_set_sensitive(op->close, FALSE);
        gtk_label_set_text(GTK_LABEL(op->status), "Running…");
        op->stream = g_data_input_stream_new(in);
        g_object_unref(in);
        op_read_next(op);
        g_child_watch_add(op->pid, op_child_exited, op);
    }
#else
    op->process = spawn_pm(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE, &error);
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
#endif
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
        GPtrArray *argv = build_argv(p->pm, p->install ? p->pm->install : p->pm->remove, p->package, TRUE);
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
                       ? "Install Chocolatey or Scoop (winget needs Windows 10 1809 or newer and is not available on Windows 7, 8 and 8.1)."
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

/* ---------- text helpers ---------- */

/* "firefox-120.0_1" -> "firefox". With last == TRUE the version starts at the last "-<digit>"
 * (xbps, pkgin: name-version); otherwise at the first one (rpm style: name-version-release.arch). */
static char *strip_version(const char *pkgver, gboolean last)
{
    const char *cut = NULL;
    for (const char *p = pkgver; *p; p++)
        if (*p == '-' && g_ascii_isdigit(p[1])) { cut = p; if (!last) break; }
    return cut ? g_strndup(pkgver, (gsize)(cut - pkgver)) : g_strdup(pkgver);
}

/* apk: "name-1.2.3-r0" -> "name" */
static char *strip_apk_version(const char *pkgver)
{
    char *name = g_strdup(pkgver), *p = strrchr(name, '-');
    if (p && p[1] == 'r' && g_ascii_isdigit(p[2])) {
        *p = 0;
        p = strrchr(name, '-');
        if (p) *p = 0;
    }
    return name;
}

/* Strips "\r"-separated progress redraws (winget/scoop spinners): keeps what was drawn last. */
static char *last_redraw(char *line)
{
    char *cr = strrchr(line, '\r');
    if (cr && cr[1]) return cr + 1;
    if (cr) *cr = 0;
    return line;
}

/* ---------- column tables (winget, scoop, snap) ---------- */

/* Character offsets at which columns start in a header: a column begins after two or more
 * spaces. Works for any language because it never looks at the header words. */
static GArray *table_columns(const char *header)
{
    GArray *cols = g_array_new(FALSE, FALSE, sizeof(glong));
    glong pos = 0;
    int spaces = 2;
    for (const char *p = header; *p; p = g_utf8_next_char(p), pos++) {
        if (*p == ' ' || *p == '\t') { spaces++; continue; }
        if (spaces >= 2) g_array_append_val(cols, pos);
        spaces = 0;
    }
    return cols;
}

static char *table_cell(const char *line, glong len, const GArray *cols, guint i)
{
    glong a, b;
    char *raw;
    if (i >= cols->len) return g_strdup("");
    a = g_array_index(cols, glong, i);
    b = (i + 1 < cols->len) ? g_array_index(cols, glong, i + 1) : len;
    if (a >= len) return g_strdup("");
    if (b > len) b = len;
    raw = g_utf8_substring(line, a, b);
    g_strstrip(raw);
    return raw;
}

static gboolean is_blank(const char *s)
{
    for (; *s; s++) if (!g_ascii_isspace(*s)) return FALSE;
    return TRUE;
}

static gboolean is_rule_line(const char *line)
{
    int dashes = 0;
    for (const char *p = line; *p; p++) {
        if (*p == '-') dashes++;
        else if (*p != ' ') return FALSE;
    }
    return dashes >= 3;
}

static void parse_table(SearchKind kind, const char *out, GPtrArray *res)
{
    char **lines = g_strsplit(out, "\n", -1);
    GArray *cols = NULL;
    const char *prev = NULL;
    char *header = NULL;

    for (guint i = 0; lines[i] && res->len < MAX_RESULTS; i++) {
        char *line = last_redraw(g_strchomp(lines[i]));
        glong len;
        char *name, *id, *summary = NULL;

        if (!cols) {
            if (kind == SEARCH_SNAP) { /* snap prints no rule line: the first line is the header */
                if (!is_blank(line)) { header = g_strdup(line); cols = table_columns(header); }
                continue;
            }
            if (is_rule_line(line) && prev) {
                header = g_strdup(prev);
                cols = table_columns(header);
            } else if (!is_blank(line)) {
                prev = line;
            }
            continue;
        }
        if (is_blank(line)) {
            if (kind == SEARCH_SCOOP) break; /* scoop ends the table with a blank line */
            continue;
        }
        if (g_str_has_prefix(line, "---")) continue;
        len = g_utf8_strlen(line, -1);

        name = table_cell(line, len, cols, 0);
        if (kind == SEARCH_WINGET) {
            char *ver;
            if (cols->len < 2) { g_free(name); break; }
            id = table_cell(line, len, cols, 1);
            ver = table_cell(line, len, cols, 2);
            if (*ver) summary = g_strdup_printf("Version %s", ver);
            g_free(ver);
        } else if (kind == SEARCH_SCOOP) {
            char *ver = table_cell(line, len, cols, 1), *bucket = table_cell(line, len, cols, 2);
            id = g_strdup(name);
            if (*ver) summary = *bucket ? g_strdup_printf("Version %s (%s)", ver, bucket) : g_strdup_printf("Version %s", ver);
            g_free(ver); g_free(bucket);
        } else { /* SEARCH_SNAP: Summary is the last column */
            id = g_strdup(name);
            summary = table_cell(line, len, cols, cols->len - 1);
            if (cols->len < 2) g_clear_pointer(&summary, g_free);
        }
        if (*id) g_ptr_array_add(res, store_entry(id, name, summary));
        g_free(name); g_free(id); g_free(summary);
    }
    g_free(header);
    if (cols) g_array_unref(cols);
    g_strfreev(lines);
}

/* ---------- entries that span several lines (pacman, nix, emerge) ---------- */

static void parse_multiline(SearchKind kind, const char *out, GPtrArray *res)
{
    char **lines = g_strsplit(out, "\n", -1);
    StoreApp *last = NULL;

    for (guint i = 0; lines[i] && res->len < MAX_RESULTS; i++) {
        char *line = g_strchomp(lines[i]);
        gboolean indented = *line == ' ' || *line == '\t';
        char *text = g_strstrip(line);
        if (!*text) continue;

        if (kind == SEARCH_PACMAN) {
            if (indented) {
                if (last && !last->summary) last->summary = g_strdup(text);
            } else {
                /* "extra/firefox 120.0-1 [installed]" -> "firefox" */
                char *space = strchr(text, ' ');
                char *first = space ? g_strndup(text, (gsize)(space - text)) : g_strdup(text);
                char *slash = strchr(first, '/');
                const char *name = slash ? slash + 1 : first;
                last = store_entry(name, name, NULL);
                g_ptr_array_add(res, last);
                g_free(first);
            }
        } else if (kind == SEARCH_NIX) {
            if (g_str_has_prefix(text, "* ")) {
                /* "* legacyPackages.x86_64-linux.firefox (120.0)" -> attribute "firefox" */
                char *attr = g_strdup(text + 2), *paren = strstr(attr, " (");
                char *dot1, *dot2, *name;
                if (paren) *paren = 0;
                dot1 = strchr(attr, '.');
                dot2 = dot1 ? strchr(dot1 + 1, '.') : NULL;
                name = dot2 ? dot2 + 1 : attr;
                last = store_entry(name, name, NULL);
                g_ptr_array_add(res, last);
                g_free(attr);
            } else if (last && !last->summary) {
                last->summary = g_strdup(text);
            }
        } else if (kind == SEARCH_EMERGE) {
            if (text[0] == '*' && text[1] == ' ') {
                char *atom = g_strstrip(text + 1), *slash = strchr(atom, '/');
                last = store_entry(atom, slash ? slash + 1 : atom, NULL);
                g_ptr_array_add(res, last);
            } else if (last && g_str_has_prefix(text, "Description:")) {
                g_free(last->summary);
                last->summary = g_strdup(g_strstrip(text + 12));
            } else if (last && g_str_has_prefix(text, "Homepage:")) {
                g_free(last->homepage);
                last->homepage = g_strdup(g_strstrip(text + 9));
            }
        }
    }
    g_strfreev(lines);
}

/* PackageKit (pkcon --plain): "Available  firefox-120.0-1.x86_64 (repo)  summary". */
static void parse_packagekit_line(const char *line, GPtrArray *res)
{
    const char *p = line, *tok;
    char *id, *name, *summary = NULL;
    gsize n;
    gboolean installed = FALSE;

    if (g_ascii_isupper(*p)) { /* leading info word: Available, Installed, ... */
        const char *w = p;
        while (*p && !g_ascii_isspace(*p)) p++;
        installed = g_str_has_prefix(w, "Installed");
        while (g_ascii_isspace(*p)) p++;
    }
    tok = p;
    while (*p && !g_ascii_isspace(*p)) p++;
    n = (gsize)(p - tok);
    if (!n || installed) return;
    id = g_strndup(tok, n);
    if (!strchr(id, ';')) {
        const char *dash = strchr(id, '-');
        gboolean versioned = FALSE;
        for (; dash; dash = strchr(dash + 1, '-'))
            if (g_ascii_isdigit(dash[1])) { versioned = TRUE; break; }
        if (!versioned) { g_free(id); return; } /* not a package line (status text) */
    }
    if (strchr(id, ';')) { *strchr(id, ';') = 0; name = g_strdup(id); }
    else name = strip_version(id, FALSE);
    while (g_ascii_isspace(*p)) p++;
    if (*p == '(') { /* "(repository)" */
        while (*p && *p != ')') p++;
        if (*p) p++;
        while (g_ascii_isspace(*p)) p++;
    }
    if (*p) summary = g_strdup(p);
    if (*name) g_ptr_array_add(res, store_entry(name, name, summary));
    g_free(id); g_free(name); g_free(summary);
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
    if (kind == SEARCH_WINGET || kind == SEARCH_SCOOP || kind == SEARCH_SNAP) { parse_table(kind, out, res); return res; }
    if (kind == SEARCH_PACMAN || kind == SEARCH_NIX || kind == SEARCH_EMERGE) { parse_multiline(kind, out, res); return res; }

    lines = g_strsplit(out, "\n", -1);
    for (guint i = 0; lines[i] && res->len < MAX_RESULTS; i++) {
        char *line = g_strstrip(last_redraw(g_strchomp(lines[i])));
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
        case SEARCH_DNF: { /* "firefox.x86_64 : Mozilla Firefox"; skips banners like "Last metadata expiration check: ..." */
            char *sep = strstr(line, " : ");
            if (sep) {
                char *name = g_strndup(line, (gsize)(sep - line)), *dot = strrchr(name, '.');
                if (!strchr(name, ' ')) {
                    if (dot && (g_str_has_prefix(dot + 1, "x86") || g_str_has_prefix(dot + 1, "aarch") ||
                                g_str_has_prefix(dot + 1, "i686") || g_str_has_prefix(dot + 1, "i386") ||
                                g_str_has_prefix(dot + 1, "noarch") || g_str_has_prefix(dot + 1, "ppc") ||
                                g_str_has_prefix(dot + 1, "s390") || g_str_has_prefix(dot + 1, "armv") ||
                                g_str_has_prefix(dot + 1, "riscv"))) *dot = 0;
                    g_ptr_array_add(res, store_entry(name, name, sep + 3));
                }
                g_free(name);
            }
            break;
        }
        case SEARCH_ZYPPER: { /* "   | name | summary | package" */
            char **c = g_strsplit(line, "|", 5);
            if (c[0] && c[1] && c[2]) {
                char *name = g_strdup(c[1]), *summary = g_strdup(c[2]);
                g_strstrip(name); g_strstrip(summary);
                if (*name && g_strcmp0(g_strstrip(c[0]), "S") != 0 && g_strcmp0(name, "Name") != 0 && *name != '-')
                    g_ptr_array_add(res, store_entry(name, name, summary));
                g_free(name); g_free(summary);
            }
            g_strfreev(c);
            break;
        }
        case SEARCH_APK: { /* "firefox-120.0-r0 - Safe and easy web browser" */
            char *sep = strstr(line, " - ");
            char *left = sep ? g_strndup(line, (gsize)(sep - line)) : g_strdup(line);
            char *name = strchr(left, ' ') ? NULL : strip_apk_version(left);
            if (name && *name) g_ptr_array_add(res, store_entry(name, name, sep ? sep + 3 : NULL));
            g_free(name); g_free(left);
            break;
        }
        case SEARCH_XBPS: { /* "[-] firefox-120.0_1   Mozilla Firefox" */
            char *p = line, *end;
            char *pkgver, *name;
            if (*p == '[') { char *close = strchr(p, ']'); if (!close) break; p = close + 1; }
            while (g_ascii_isspace(*p)) p++;
            end = p;
            while (*end && !g_ascii_isspace(*end)) end++;
            if (end == p) break;
            pkgver = g_strndup(p, (gsize)(end - p));
            name = strip_version(pkgver, TRUE);
            while (g_ascii_isspace(*end)) end++;
            g_ptr_array_add(res, store_entry(name, name, *end ? end : NULL));
            g_free(pkgver); g_free(name);
            break;
        }
        case SEARCH_PKGIN: { /* "firefox-120.0nb1  Web browser" ; legend lines look like "=: installed" */
            char *p = line, *end, *pkgver, *name;
            if (line[1] == ':') break;
            end = p;
            while (*end && !g_ascii_isspace(*end)) end++;
            if (end == p) break;
            pkgver = g_strndup(p, (gsize)(end - p));
            name = strip_version(pkgver, TRUE);
            while (g_ascii_isspace(*end) || *end == '=' || *end == '<' || *end == '>') end++;
            g_ptr_array_add(res, store_entry(name, name, *end ? end : NULL));
            g_free(pkgver); g_free(name);
            break;
        }
        case SEARCH_PKGKIT:
            parse_packagekit_line(line, res);
            break;
        default: break;
        }
    }
    g_strfreev(lines);
    return res;
}

/* ======================================================================
 * Search
 * ====================================================================== */

typedef struct {
    GtkWidget *flow, *status;
    GtkWindow *window;
    guint generation;
    const PackageManager *pm; /* NULL for the AppStream fallback */
    SearchKind kind;
    char *query;
    gboolean may_fall_back;   /* PackageKit -> native package manager */
} SearchCtx;

static guint search_generation;

static void clear_flow(GtkWidget *flow)
{
    GtkWidget *c;
    while ((c = gtk_widget_get_first_child(flow)) != NULL) gtk_flow_box_remove(GTK_FLOW_BOX(flow), c);
}

static void search_ctx_free(SearchCtx *ctx)
{
    if (ctx->flow) g_object_remove_weak_pointer(G_OBJECT(ctx->flow), (gpointer *)&ctx->flow);
    if (ctx->status) g_object_remove_weak_pointer(G_OBJECT(ctx->status), (gpointer *)&ctx->status);
    g_free(ctx->query);
    g_free(ctx);
}

/* The first detected package manager other than `not_this` that can search. */
static const PackageManager *native_fallback(const PackageManager *not_this, guint *index)
{
    if (!detected) return NULL;
    for (guint i = 0; i < detected->len; i++) {
        const PackageManager *pm = g_ptr_array_index(detected, i);
        if (pm != not_this && pm->search && pm->kind != SEARCH_NONE) { if (index) *index = i; return pm; }
    }
    return NULL;
}

static void search_complete(SearchCtx *ctx, gboolean ok, GBytes *out, GError *error);

#ifdef G_OS_WIN32
typedef struct { SearchCtx *ctx; GInputStream *in; GByteArray *buf; GPid pid; } WinRead;
static void win_read_chunk(GObject *src, GAsyncResult *res, gpointer data);
#else
static void search_done(GObject *src, GAsyncResult *res, gpointer data);
#endif

/* Starts one search process. Returns FALSE (and fills the status label) when it cannot start. */
static gboolean launch_search(const PackageManager *pm, SearchKind kind, GPtrArray *argv, const char *query,
                              GtkWidget *flow, GtkWidget *status, GtkWindow *window, gboolean may_fall_back)
{
#ifdef G_OS_WIN32
    GPtrArray *prepared;
    GInputStream *in;
    GPid pid = 0;
#else
    GSubprocess *proc;
#endif
    GError *error = NULL;
    SearchCtx *ctx;

#ifdef G_OS_WIN32
    prepared = win_prepare(argv, &error);
    in = prepared ? lh4051_win_spawn((const char *const *)prepared->pdata, FALSE, &pid, &error) : NULL;
    if (prepared) g_ptr_array_unref(prepared);
    if (!in) {
        gtk_label_set_text(GTK_LABEL(status), error ? error->message : "Could not start the search.");
        g_clear_error(&error);
        return FALSE;
    }
#else
    proc = spawn_pm(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, &error);
    if (!proc) {
        gtk_label_set_text(GTK_LABEL(status), error ? error->message : "Could not start the search.");
        g_clear_error(&error);
        return FALSE;
    }
#endif
    ctx = g_new0(SearchCtx, 1);
    ctx->flow = flow; ctx->status = status; ctx->window = window; ctx->kind = kind; ctx->pm = pm;
    ctx->query = g_strdup(query);
    ctx->may_fall_back = may_fall_back;
    ctx->generation = search_generation;
    g_object_add_weak_pointer(G_OBJECT(flow), (gpointer *)&ctx->flow);
    g_object_add_weak_pointer(G_OBJECT(status), (gpointer *)&ctx->status);
#ifdef G_OS_WIN32
    {
        WinRead *r = g_new0(WinRead, 1);
        r->ctx = ctx; r->in = in; r->pid = pid; r->buf = g_byte_array_new();
        g_input_stream_read_bytes_async(in, 8192, G_PRIORITY_DEFAULT, NULL, win_read_chunk, r);
    }
#else
    g_subprocess_communicate_async(proc, NULL, NULL, search_done, ctx);
#endif
    return TRUE;
}

/* Shows the results of a finished search (or starts the PackageKit -> native fallback) and frees ctx.
 * `out` and `error` stay owned by the caller. */
static void search_complete(SearchCtx *ctx, gboolean ok, GBytes *out, GError *error)
{
    if (!ctx->flow || !ctx->status || ctx->generation != search_generation) {
        /* window closed or a newer search superseded this one */
    } else {
        char *text = ok ? output_to_utf8(out) : NULL;
        GPtrArray *results = ok ? parse_results(ctx->kind, text) : g_ptr_array_new_with_free_func((GDestroyNotify)store_app_free);
        guint n = results->len;
        guint fb_index = 0;
        const PackageManager *fb = (!n && ctx->may_fall_back) ? native_fallback(ctx->pm, &fb_index) : NULL;
        gboolean handled = FALSE;

        if (fb) {
            /* PackageKit is missing, not running or found nothing: ask a native package manager. */
            GPtrArray *argv = build_argv(fb, fb->search, ctx->query, FALSE);
            char *m = g_strdup_printf("PackageKit returned no results, searching with %s…", fb->label);
            gtk_label_set_text(GTK_LABEL(ctx->status), m);
            g_free(m);
            if (pm_dropdown) gtk_drop_down_set_selected(GTK_DROP_DOWN(pm_dropdown), fb_index);
            handled = launch_search(fb, fb->kind, argv, ctx->query, ctx->flow, ctx->status, ctx->window, FALSE);
            g_ptr_array_unref(argv);
        }
        if (!handled) {
            for (guint i = 0; i < n; i++)
                gtk_flow_box_insert(GTK_FLOW_BOX(ctx->flow),
                                    make_card(g_ptr_array_steal_index(results, 0), ctx->window, FALSE), -1);
            if (!ok) gtk_label_set_text(GTK_LABEL(ctx->status), error ? error->message : "Search failed.");
            else {
                char *m = n ? g_strdup_printf("%u result%s", n, n == 1 ? "" : "s") : g_strdup("No matching applications.");
                gtk_label_set_text(GTK_LABEL(ctx->status), m);
                g_free(m);
            }
        }
        g_ptr_array_unref(results);
        g_free(text);
    }
    search_ctx_free(ctx);
}

#ifdef G_OS_WIN32
static void win_read_chunk(GObject *src, GAsyncResult *res, gpointer data)
{
    WinRead *r = data;
    GError *error = NULL;
    GBytes *chunk = g_input_stream_read_bytes_finish(G_INPUT_STREAM(src), res, &error);

    if (chunk && g_bytes_get_size(chunk) > 0) {
        gsize n;
        const guint8 *d = g_bytes_get_data(chunk, &n);
        g_byte_array_append(r->buf, d, (guint)n);
        g_bytes_unref(chunk);
        g_input_stream_read_bytes_async(r->in, 8192, G_PRIORITY_DEFAULT, NULL, win_read_chunk, r);
        return;
    }
    if (chunk) g_bytes_unref(chunk);
    {
        GBytes *out = g_byte_array_free_to_bytes(r->buf); /* EOF (or an error): the child closed its output */
        search_complete(r->ctx, error == NULL, out, error);
        g_bytes_unref(out);
    }
    g_clear_error(&error);
    g_spawn_close_pid(r->pid);
    g_object_unref(r->in);
    g_free(r);
}
#else
static void search_done(GObject *src, GAsyncResult *res, gpointer data)
{
    SearchCtx *ctx = data;
    GBytes *out = NULL, *err = NULL;
    GError *error = NULL;
    gboolean ok = g_subprocess_communicate_finish(G_SUBPROCESS(src), res, &out, &err, &error);

    search_complete(ctx, ok, out, error);
    g_clear_error(&error);
    if (out) g_bytes_unref(out);
    if (err) g_bytes_unref(err);
    g_object_unref(src);
}
#endif

static void do_search(GtkWidget *entry, GtkWidget *flow, GtkWidget *status, GtkWindow *window)
{
    const char *query = gtk_editable_get_text(GTK_EDITABLE(entry));
    const PackageManager *pm = current_pm();
    GPtrArray *argv = NULL;
    SearchKind kind = SEARCH_NONE;
    gboolean ok;

    search_generation++;
    clear_flow(flow);
    if (!query || !*query) { gtk_label_set_text(GTK_LABEL(status), "Type a name to search."); return; }

    if (pm && pm->search && pm->kind != SEARCH_NONE) {
        argv = build_argv(pm, pm->search, query, FALSE);
        kind = pm->kind;
    } else if (!lh4051_platform_is_windows() && g_find_program_in_path("appstreamcli")) {
        pm = NULL;
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

    gtk_label_set_text(GTK_LABEL(status), "Searching…");
    ok = launch_search(pm, kind, argv, query, flow, status, window, pm && kind == SEARCH_PKGKIT);
    g_ptr_array_unref(argv);
    (void)ok;
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
    title = gtk_label_new("App Store");
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
    gtk_widget_add_css_class(row, "lh-store-searchbar");
    gtk_widget_add_css_class(pm_row, "lh-store-pmrow");
    gtk_widget_add_css_class(pm_label, "lh-store-pmlabel");
    gtk_widget_add_css_class(body, "lh-store-body");
    gtk_widget_add_css_class(rscroll, "lh-store-pane");
    gtk_widget_add_css_class(iscroll, "lh-store-pane");
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
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(results), 1);
    gtk_widget_set_vexpand(rscroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(rscroll), results);
    gtk_box_append(GTK_BOX(body), rscroll);
    gtk_box_append(GTK_BOX(body), ilabel);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(installed), GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(installed), 1);
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
