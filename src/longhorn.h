#ifndef LONGHORN_H
#define LONGHORN_H

#include <gtk/gtk.h>

#define LH4051_VERSION "0.0.0"
#define LH4051_RES "/org/kuznix/lh4051de/"
#define LH4051_PANEL_HEIGHT 40
#define LH4051_SIDEBAR_WIDTH 240
#define LH4051_SIDEBAR_COLLAPSED 22

#if !GLIB_CHECK_VERSION(2, 74, 0)
#define G_APPLICATION_DEFAULT_FLAGS G_APPLICATION_FLAGS_NONE
#endif

/* theme.c */
void lh4051_theme_load(void);

/* platform.c */
gboolean lh4051_platform_is_windows(void);
const char *lh4051_platform_backend(GdkDisplay *display);
void lh4051_open_uri(const char *uri);
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
GtkWidget *lh4051_panel_new(GtkApplication *app);
GtkWidget *lh4051_sidebar_new(GtkApplication *app);
GtkWidget *lh4051_start_menu_new(GtkApplication *app);
void lh4051_start_menu_reset(void);

/* applications */
void lh4051_explorer_open(GtkApplication *app, const char *path);
void lh4051_explorer_open_search(GtkApplication *app, const char *path, const char *query);
void lh4051_settings_open(GtkApplication *app);
void lh4051_appstore_open(GtkApplication *app);

#endif
