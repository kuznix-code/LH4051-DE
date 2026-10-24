#include "longhorn.h"
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

void lh4051_open_uri(const char *uri)
{
    GError *error = NULL;
    if (!uri) return;
    if (!g_app_info_launch_default_for_uri(uri, NULL, &error)) {
        g_warning("LH4051: cannot open %s: %s", uri, error ? error->message : "unknown error");
        g_clear_error(&error);
    }
}

void lh4051_open_recycle_bin(void)
{
#ifdef G_OS_WIN32
    ShellExecuteA(NULL, "open", "shell:RecycleBinFolder", NULL, NULL, SW_SHOWNORMAL);
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
