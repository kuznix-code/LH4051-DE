#include "longhorn.h"

static const char *keys[LH_PREF_COUNT] = {"desktop-icons", "watermark", "sidebar", "clock-24h", "clock-seconds"};
static gboolean values[LH_PREF_COUNT] = {TRUE, TRUE, TRUE, FALSE, FALSE};
static char *wallpaper;

typedef struct { LHPrefsFunc func; gpointer data; } Watcher;
static GArray *watchers;

static char *prefs_path(void)
{
    return g_build_filename(g_get_user_config_dir(), "lh4051-de", "settings.ini", NULL);
}

void lh4051_prefs_load(void)
{
    char *path = prefs_path();
    GKeyFile *kf = g_key_file_new();
    if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)) {
        for (int i = 0; i < LH_PREF_COUNT; i++) {
            GError *e = NULL;
            gboolean v = g_key_file_get_boolean(kf, "Shell", keys[i], &e);
            if (!e) values[i] = v;
            g_clear_error(&e);
        }
        g_free(wallpaper);
        wallpaper = g_key_file_get_string(kf, "Shell", "wallpaper", NULL);
        if (wallpaper && !*wallpaper) g_clear_pointer(&wallpaper, g_free);
    }
    g_key_file_free(kf);
    g_free(path);
}

static void prefs_save(void)
{
    char *path = prefs_path();
    char *dir = g_path_get_dirname(path);
    GKeyFile *kf = g_key_file_new();
    g_mkdir_with_parents(dir, 0700);
    for (int i = 0; i < LH_PREF_COUNT; i++)
        g_key_file_set_boolean(kf, "Shell", keys[i], values[i]);
    g_key_file_set_string(kf, "Shell", "wallpaper", wallpaper ? wallpaper : "");
    if (!g_key_file_save_to_file(kf, path, NULL))
        g_warning("LH4051: could not save %s", path);
    g_key_file_free(kf);
    g_free(dir);
    g_free(path);
}

static void notify(void)
{
    if (!watchers) return;
    for (guint i = 0; i < watchers->len; i++) {
        Watcher *w = &g_array_index(watchers, Watcher, i);
        w->func(w->data);
    }
}

gboolean lh4051_prefs_get(LHPref key) { return key < LH_PREF_COUNT ? values[key] : FALSE; }

void lh4051_prefs_set(LHPref key, gboolean value)
{
    if (key >= LH_PREF_COUNT || values[key] == value) return;
    values[key] = value;
    prefs_save();
    notify();
}

const char *lh4051_prefs_wallpaper(void) { return wallpaper; }

void lh4051_prefs_set_wallpaper(const char *path)
{
    g_free(wallpaper);
    wallpaper = (path && *path) ? g_strdup(path) : NULL;
    prefs_save();
    notify();
}

void lh4051_prefs_watch(LHPrefsFunc func, gpointer user_data)
{
    Watcher w = {func, user_data};
    if (!watchers) watchers = g_array_new(FALSE, FALSE, sizeof(Watcher));
    g_array_append_val(watchers, w);
}
