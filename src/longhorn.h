#ifndef LONGHORN_H
#define LONGHORN_H

#include <gtk/gtk.h>

#define LH4051_VERSION "0.2.0.dev"
#define LH4051_RES "/org/kuznix/lh4051de/"
#define LH4051_PANEL_HEIGHT 40
#define LH4051_SIDEBAR_WIDTH 240
#define LH4051_SIDEBAR_COLLAPSED 22

/* OS family (compile time). Windows is detected through G_OS_WIN32. */
#if defined(__APPLE__) && defined(__MACH__)
#define LH_OS_MACOS 1
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#define LH_OS_BSD 1
#endif

#if !GLIB_CHECK_VERSION(2, 74, 0)
#define G_APPLICATION_DEFAULT_FLAGS G_APPLICATION_FLAGS_NONE
#endif

/* theme.c */
void lh4051_theme_load(void);

/* platform.c */
gboolean lh4051_platform_is_windows(void);
const char *lh4051_platform_name(void); /* linux, windows, macos, freebsd, openbsd, netbsd, dragonfly, unix */
const char *lh4051_platform_backend(GdkDisplay *display);
gboolean lh4051_open_uri_checked(const char *uri, GError **error);
void lh4051_open_uri(const char *uri);

/* Installed applications, sorted by name. Linux/BSD/Windows use GIO app
 * registration; macOS also scans /Applications and ~/Applications. */
typedef struct {
    char *name;
    char *summary;
    char *id;
    GIcon *icon;
    GAppInfo *info;   /* GIO-registered application, or NULL */
    char *path;       /* .app bundle path (macOS), or NULL */
} LHApp;
GPtrArray *lh4051_apps_list(void); /* free with g_ptr_array_unref */
gboolean lh4051_app_launch(const LHApp *app, GError **error);
void lh4051_open_recycle_bin(void);
char *lh4051_computer_root(void);
const char *lh4051_user_dir(GUserDirectory dir);
char *lh4051_data_dir(void);

/* prefs.c */
typedef enum {
    LH_PREF_DESKTOP_ICONS,
    LH_PREF_WATERMARK,
    LH_PREF_SIDEBAR,
    LH_PREF_CLOCK_24H,
    LH_PREF_COUNT
} LHPref;
typedef void (*LHPrefsFunc)(gpointer user_data);
void lh4051_prefs_load(void);
gboolean lh4051_prefs_get(LHPref key);
void lh4051_prefs_set(LHPref key, gboolean value);
const char *lh4051_prefs_wallpaper(void);
void lh4051_prefs_set_wallpaper(const char *path);
void lh4051_prefs_watch(LHPrefsFunc func, gpointer user_data);

/* shell widgets */
GtkWidget *lh4051_desktop_new(GtkApplication *app);
void lh4051_desktop_refresh(void);
void lh4051_desktop_set_sidebar_inset(int px, gboolean animate);
GtkWidget *lh4051_panel_new(GtkApplication *app);
GtkWidget *lh4051_sidebar_new(GtkApplication *app);
void lh4051_sidebar_sync_desktop(gboolean animate);
GtkWidget *lh4051_start_menu_new(GtkApplication *app);
void lh4051_start_menu_reset(void);

/* applications */
void lh4051_explorer_open(GtkApplication *app, const char *path);
void lh4051_explorer_open_search(GtkApplication *app, const char *path, const char *query);
void lh4051_settings_open(GtkApplication *app);
void lh4051_appstore_open(GtkApplication *app);

#endif
