#include "longhorn.h"
#include <string.h>
#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

gboolean lh4051_platform_is_windows(void)
{
#ifdef G_OS_WIN32
    return TRUE;
#else
    return FALSE;
#endif
}

const char *lh4051_platform_name(void)
{
#if defined(G_OS_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__FreeBSD__)
    return "freebsd";
#elif defined(__OpenBSD__)
    return "openbsd";
#elif defined(__NetBSD__)
    return "netbsd";
#elif defined(__DragonFly__)
    return "dragonfly";
#elif defined(__linux__)
    return "linux";
#else
    return "unix";
#endif
}

/* Detect the GDK backend by type name so no backend-specific headers are
 * needed (keeps MinGW/MSVC/cross builds independent of X11/Wayland). */
const char *lh4051_platform_backend(GdkDisplay *display)
{
    const char *type = display ? G_OBJECT_TYPE_NAME(display) : NULL;
    if (!type) return "unknown";
    if (g_strstr_len(type, -1, "Wayland")) return "wayland";
    if (g_strstr_len(type, -1, "X11")) return "x11";
    if (g_strstr_len(type, -1, "Win32")) return "win32";
    if (g_strstr_len(type, -1, "Macos")) return "macos";
    return "other";
}

static gboolean spawn_simple(const char *a0, const char *a1, GError **error)
{
    const char *argv[] = {a0, a1, NULL};
    return g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, error);
}

gboolean lh4051_open_uri_checked(const char *uri, GError **error)
{
    if (!uri) return FALSE;
#if defined(LH_OS_MACOS)
    return spawn_simple("open", uri, error);
#else
    {
        GError *local = NULL;
        if (g_app_info_launch_default_for_uri(uri, NULL, &local)) return TRUE;
#if !defined(G_OS_WIN32)
        /* Minimal systems may have no registered handler but still ship xdg-open. */
        if (g_find_program_in_path("xdg-open")) {
            g_clear_error(&local);
            return spawn_simple("xdg-open", uri, error);
        }
#endif
        g_propagate_error(error, local);
        return FALSE;
    }
#endif
}

void lh4051_open_uri(const char *uri)
{
    GError *error = NULL;
    if (!lh4051_open_uri_checked(uri, &error)) {
        g_warning("LH4051: cannot open %s: %s", uri ? uri : "(null)", error ? error->message : "unknown error");
        g_clear_error(&error);
    }
}

void lh4051_open_recycle_bin(void)
{
#if defined(G_OS_WIN32)
    ShellExecuteA(NULL, "open", "shell:RecycleBinFolder", NULL, NULL, SW_SHOWNORMAL);
#elif defined(LH_OS_MACOS)
    char *trash = g_build_filename(g_get_home_dir(), ".Trash", NULL);
    spawn_simple("open", trash, NULL);
    g_free(trash);
#else
    lh4051_open_uri("trash:///");
#endif
}

char *lh4051_computer_root(void)
{
#ifdef G_OS_WIN32
    return g_strdup("C:\\");
#else
    return g_strdup("/");
#endif
}

const char *lh4051_user_dir(GUserDirectory dir)
{
    const char *p = g_get_user_special_dir(dir);
    return p ? p : g_get_home_dir();
}

/* Per-user data directory (created on demand). */
char *lh4051_data_dir(void)
{
    char *dir = g_build_filename(g_get_user_data_dir(), "lh4051-de", NULL);
    g_mkdir_with_parents(dir, 0700);
    return dir;
}

/* ---------- installed applications ---------- */

static void app_free(LHApp *a)
{
    if (!a) return;
    g_free(a->name); g_free(a->summary); g_free(a->id); g_free(a->path);
    g_clear_object(&a->icon);
    g_clear_object(&a->info);
    g_free(a);
}

static gint app_compare(gconstpointer a, gconstpointer b)
{
    const LHApp *x = *(LHApp *const *)a, *y = *(LHApp *const *)b;
    return g_utf8_collate(x->name, y->name);
}

#ifdef LH_OS_MACOS
static void scan_app_bundles(GPtrArray *list, const char *dir)
{
    GDir *d = g_dir_open(dir, 0, NULL);
    const char *name;
    if (!d) return;
    while ((name = g_dir_read_name(d)) != NULL) {
        if (g_str_has_suffix(name, ".app")) {
            LHApp *a = g_new0(LHApp, 1);
            a->name = g_strndup(name, strlen(name) - 4);
            a->path = g_build_filename(dir, name, NULL);
            a->id = g_strdup(a->path);
            g_ptr_array_add(list, a);
        }
    }
    g_dir_close(d);
}
#endif

GPtrArray *lh4051_apps_list(void)
{
    GPtrArray *list = g_ptr_array_new_with_free_func((GDestroyNotify)app_free);
    GList *apps = g_app_info_get_all();

    for (GList *l = apps; l; l = l->next) {
        GAppInfo *info = l->data;
        LHApp *a;
        if (!g_app_info_should_show(info) || !g_app_info_get_display_name(info)) continue;
        a = g_new0(LHApp, 1);
        a->name = g_strdup(g_app_info_get_display_name(info));
        a->summary = g_strdup(g_app_info_get_description(info));
        a->id = g_strdup(g_app_info_get_id(info));
        a->icon = g_app_info_get_icon(info) ? g_object_ref(g_app_info_get_icon(info)) : NULL;
        a->info = g_object_ref(info);
        g_ptr_array_add(list, a);
    }
    g_list_free_full(apps, g_object_unref);

#ifdef LH_OS_MACOS
    scan_app_bundles(list, "/Applications");
    scan_app_bundles(list, "/System/Applications");
    {
        char *user = g_build_filename(g_get_home_dir(), "Applications", NULL);
        scan_app_bundles(list, user);
        g_free(user);
    }
#endif
    g_ptr_array_sort(list, app_compare);
    return list;
}

gboolean lh4051_app_launch(const LHApp *app, GError **error)
{
    if (!app) return FALSE;
    if (app->info) return g_app_info_launch(app->info, NULL, NULL, error);
    if (app->path) {
        const char *argv[] = {"open", "-a", app->path, NULL};
        return g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, error);
    }
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Application cannot be launched.");
    return FALSE;
}
