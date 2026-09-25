#include "fm.h"

static gint compare_names(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    return g_ascii_strcasecmp((const char *)a, (const char *)b);
}

GPtrArray *lh4051_fm_list(const char *path)
{
    GPtrArray *items = g_ptr_array_new_with_free_func(g_free);
    GDir *dir;

    if (!path || !*path)
        return items;

    dir = g_dir_open(path, 0, NULL);
    if (!dir)
        return items;

    for (const char *name = g_dir_read_name(dir);
         name != NULL;
         name = g_dir_read_name(dir)) {
        if (name[0] == '.')
            continue;

        g_ptr_array_add(items, g_strdup(name));
    }

    g_dir_close(dir);
    g_ptr_array_sort_with_data(items, compare_names, NULL);
    return items;
}

void lh4051_fm_free_list(GPtrArray *items)
{
    if (items)
        g_ptr_array_unref(items);
}
