#include "longhorn.h"
#include <string.h>
#ifndef G_OS_WIN32
#include <unistd.h>
#endif

/* ----------------------------------------------------------------------------
 * GUI updater policy.
 *
 * The updater is OFF by default (GUI and the lh4051-de-updater command) wherever the system has a package
 * manager that should update LH4051-DE. It is ON by default on Windows, on macOS, and on Unix-like systems
 * without a package manager (Linux From Scratch and similar). It can be switched on for testing with
 *   - Settings > Updates > "Enable the updater for testing"   (settings.ini: updater-testing=true)
 *   - LH4051_UPDATER=1 in the environment, or  enabled=yes  in updater.conf
 * and switched off with LH4051_UPDATER=0.
 * The same rules (and the same package-manager list) live in scripts/lh4051-de-updater; keep them in sync.
 * ---------------------------------------------------------------------------- */

typedef struct { const char *id, *label; } UpdateMode;
static const UpdateMode MODES[] = {
    {"stable", "Stable (recommended)"},
    {"lts",    "Long-term support"},
    {"beta",   "Beta"},
    {"dev",    "Development builds"},
};

/* System package managers (not Flatpak/Snap/Homebrew, which do not manage the base system). */
static const char *const PACKAGE_MANAGERS[] = {
    "apt-get", "apt", "dpkg", "dnf", "yum", "rpm", "zypper", "pacman", "apk", "xbps-install", "emerge",
    "eopkg", "slackpkg", "installpkg", "swupd", "nix-env", "guix", "pkg", "pkgin", "pkg_add", "opkg", NULL
};

static gboolean env_is(const char *name, const char *list)
{
    const char *v = g_getenv(name);
    char *low, **words;
    gboolean hit = FALSE;
    if (!v || !*v) return FALSE;
    low = g_ascii_strdown(v, -1);
    words = g_strsplit(list, " ", -1);
    for (int i = 0; words[i]; i++)
        if (!strcmp(low, words[i])) hit = TRUE;
    g_strfreev(words);
    g_free(low);
    return hit;
}

#ifndef G_OS_WIN32
static gboolean is_lfs(void)
{
    char *id, *name;
    gboolean lfs = FALSE;
    if (g_getenv("LH4051_LFS") && *g_getenv("LH4051_LFS")) return TRUE;
    if (g_file_test("/etc/lfs-release", G_FILE_TEST_EXISTS) || g_file_test("/etc/lfs-version", G_FILE_TEST_EXISTS)) return TRUE;
    id = g_get_os_info(G_OS_INFO_KEY_ID);
    name = g_get_os_info(G_OS_INFO_KEY_NAME);
    if (id && !g_ascii_strcasecmp(id, "lfs")) lfs = TRUE;
    if (name && (!g_ascii_strcasecmp(name, "lfs") || !g_ascii_strcasecmp(name, "linux from scratch"))) lfs = TRUE;
    g_free(id);
    g_free(name);
    return lfs;
}

static const char *find_package_manager(void)
{
    for (int i = 0; PACKAGE_MANAGERS[i]; i++) {
        char *p = g_find_program_in_path(PACKAGE_MANAGERS[i]);
        if (p) { g_free(p); return PACKAGE_MANAGERS[i]; }
    }
    return NULL;
}
#endif

/* The policy before any testing switch: is the updater on for this system? */
static gboolean default_on(char **note)
{
#if defined(G_OS_WIN32)
    *note = g_strdup("Windows has no package manager that keeps LH4051-DE up to date, so the updater is available.");
    return TRUE;
#elif defined(LH_OS_MACOS)
    *note = g_strdup("macOS has no package manager that keeps LH4051-DE up to date, so the updater is available.");
    return TRUE;
#else
    const char *pm;
    if (is_lfs()) {
        *note = g_strdup("Linux From Scratch has no package manager, so the updater is available.");
        return TRUE;
    }
    pm = find_package_manager();
    if (pm) {
        *note = g_strdup_printf("The updater is off because this system has a package manager (%s). "
                                "Update LH4051-DE with it.", pm);
        return FALSE;
    }
    *note = g_strdup("This system has no package manager, so the updater is available.");
    return TRUE;
#endif
}

LHUpdaterPolicy lh4051_updater_policy(char **note)
{
    char *n = NULL;
    gboolean on;

    if (env_is("LH4051_UPDATER", "0 off no false disabled")) {
        if (note) *note = g_strdup("The updater is switched off by LH4051_UPDATER.");
        return LH_UPDATER_DISABLED;
    }
    on = default_on(&n);
    if (on) {
        if (note) *note = n; else g_free(n);
        return LH_UPDATER_DEFAULT;
    }
    if (env_is("LH4051_UPDATER", "1 on yes true testing enabled") || lh4051_prefs_get(LH_PREF_UPDATER_TESTING)) {
        g_free(n);
        if (note) *note = g_strdup("The updater is switched on for testing. This system has a package manager, "
                                   "which is the supported way to update LH4051-DE.");
        return LH_UPDATER_TESTING;
    }
    if (note) *note = n; else g_free(n);
    return LH_UPDATER_DISABLED;
}

/* ------------------------------------------------------------------ mode (updater.conf: mode=...) */

static char *conf_path(void)
{
    return g_build_filename(g_get_user_config_dir(), "lh4051-de", "updater.conf", NULL);
}

static gboolean mode_valid(const char *id)
{
    for (guint i = 0; i < G_N_ELEMENTS(MODES); i++)
        if (id && !strcmp(id, MODES[i].id)) return TRUE;
    return FALSE;
}

const char *lh4051_updater_mode(void)
{
    static char mode[16];
    char *path = conf_path(), *text = NULL;
    g_strlcpy(mode, "stable", sizeof mode);
    if (g_file_get_contents(path, &text, NULL, NULL)) {
        char **lines = g_strsplit(text, "\n", -1);
        for (int i = 0; lines[i]; i++) {
            char *l = g_strstrip(lines[i]);
            if (g_str_has_prefix(l, "mode")) {
                char *eq = strchr(l, '=');
                if (eq) {
                    char *v = g_strstrip(eq + 1);
                    if (mode_valid(v)) g_strlcpy(mode, v, sizeof mode);
                }
            }
        }
        g_strfreev(lines);
        g_free(text);
    }
    g_free(path);
    return mode;
}

void lh4051_updater_set_mode(const char *id)
{
    char *path, *dir, *text = NULL;
    GString *out = g_string_new(NULL);
    gboolean replaced = FALSE;

    if (!mode_valid(id)) return;
    path = conf_path();
    dir = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0700);
    if (g_file_get_contents(path, &text, NULL, NULL)) {
        char **lines = g_strsplit(text, "\n", -1);
        for (int i = 0; lines[i]; i++) {
            char *t = g_strstrip(g_strdup(lines[i]));
            gboolean is_mode = g_str_has_prefix(t, "mode") && strchr(t, '=');
            g_free(t);
            if (is_mode) {
                if (!replaced) g_string_append_printf(out, "mode=%s\n", id);
                replaced = TRUE;
            } else if (lines[i + 1] || *lines[i]) {
                g_string_append_printf(out, "%s\n", lines[i]);
            }
        }
        g_strfreev(lines);
        g_free(text);
    }
    if (!replaced) g_string_append_printf(out, "mode=%s\n", id);
    if (!g_file_set_contents(path, out->str, -1, NULL))
        g_warning("LH4051: could not save %s", path);
    g_string_free(out, TRUE);
    g_free(dir);
    g_free(path);
}

guint lh4051_updater_mode_count(void) { return G_N_ELEMENTS(MODES); }

void lh4051_updater_mode_info(guint i, const char **id, const char **label)
{
    if (i >= G_N_ELEMENTS(MODES)) i = 0;
    if (id) *id = MODES[i].id;
    if (label) *label = MODES[i].label;
}

/* ------------------------------------------------------------------ command line */

#ifndef G_OS_WIN32
static char *find_updater_script(void)
{
    const char *res = g_getenv("LH4051_RESOURCES"); /* set by the macOS app launcher */
    const char *dirs[] = {"/usr/local/bin", "/usr/bin", "/opt/homebrew/bin", "/opt/local/bin", NULL};
    char *path;

    if (res && *res) {
        path = g_build_filename(res, "lh4051-de-updater", NULL);
        if (g_file_test(path, G_FILE_TEST_IS_EXECUTABLE)) return path;
        g_free(path);
    }
    path = g_find_program_in_path("lh4051-de-updater");
    if (path) return path;
    for (int i = 0; dirs[i]; i++) {
        path = g_build_filename(dirs[i], "lh4051-de-updater", NULL);
        if (g_file_test(path, G_FILE_TEST_IS_EXECUTABLE)) return path;
        g_free(path);
    }
    path = g_build_filename(g_get_home_dir(), ".local", "bin", "lh4051-de-updater", NULL);
    if (g_file_test(path, G_FILE_TEST_IS_EXECUTABLE)) return path;
    g_free(path);
    return NULL;
}
#endif

static void add(GPtrArray *a, const char *s) { g_ptr_array_add(a, g_strdup(s)); }

GPtrArray *lh4051_updater_argv(gboolean install, char **problem)
{
    GPtrArray *a = g_ptr_array_new_with_free_func(g_free);
    const char *mode = lh4051_updater_mode();

#ifdef G_OS_WIN32
    char *dir = g_win32_get_package_installation_directory_of_module(NULL);
    char *ps1 = dir ? g_build_filename(dir, "lh4051-de-updater.ps1", NULL) : NULL;
    g_free(dir);
    if (!ps1 || !g_file_test(ps1, G_FILE_TEST_EXISTS)) {
        if (problem) *problem = g_strdup("The updater script (lh4051-de-updater.ps1) was not found next to lh4051-de.exe.");
        g_free(ps1);
        g_ptr_array_unref(a);
        return NULL;
    }
    add(a, "powershell.exe");
    add(a, "-NoProfile");
    add(a, "-ExecutionPolicy");
    add(a, "Bypass");
    add(a, "-File");
    add(a, ps1);
    add(a, "-Mode");
    add(a, mode);
    add(a, install ? "-Yes" : "-Check");
    g_free(ps1);
#else
    char *script = find_updater_script();
    if (!script) {
        if (problem) *problem = g_strdup("The lh4051-de-updater command was not found. Reinstall LH4051-DE or add it to PATH.");
        g_ptr_array_unref(a);
        return NULL;
    }
    if (install && geteuid() != 0) {
#ifdef LH_OS_MACOS
        /* Ask for the administrator password with the system dialog (sudo needs a terminal). */
        char *quoted = g_shell_quote(script), *cmd, *escaped, *osa;
        GString *e = g_string_new(NULL);
        cmd = g_strdup_printf("%s --mode %s --yes", quoted, mode);
        for (const char *c = cmd; *c; c++) {
            if (*c == '\\' || *c == '"') g_string_append_c(e, '\\');
            g_string_append_c(e, *c);
        }
        escaped = g_string_free(e, FALSE);
        osa = g_strdup_printf("do shell script \"%s\" with administrator privileges", escaped);
        add(a, "osascript");
        add(a, "-e");
        g_ptr_array_add(a, osa);
        g_free(quoted); g_free(cmd); g_free(escaped);
        g_free(script);
        g_ptr_array_add(a, NULL);
        return a;
#else
        char *pk = g_find_program_in_path("pkexec");
        if (!pk) {
            if (problem) *problem = g_strdup_printf("Installing needs administrator rights and pkexec was not found. "
                                                    "Run this in a terminal: %s --mode %s --yes", script, mode);
            g_free(script);
            g_ptr_array_unref(a);
            return NULL;
        }
        add(a, pk);
        g_free(pk);
#endif
    }
    add(a, script);
    add(a, "--mode");
    add(a, mode);
    add(a, install ? "--yes" : "--check");
    g_free(script);
#endif
    g_ptr_array_add(a, NULL);
    return a;
}
