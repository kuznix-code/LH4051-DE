#include "fm.h"

static gint compare_items(gconstpointer a, gconstpointer b, gpointer user_data)
{
    const LH4051FMItem *ia = *(LH4051FMItem * const *)a;
    const LH4051FMItem *ib = *(LH4051FMItem * const *)b;
    (void)user_data;

    if (ia->type == G_FILE_TYPE_DIRECTORY && ib->type != G_FILE_TYPE_DIRECTORY)
        return -1;
    if (ia->type != G_FILE_TYPE_DIRECTORY && ib->type == G_FILE_TYPE_DIRECTORY)
        return 1;
    return g_ascii_strcasecmp(ia->name, ib->name);
}

void lh4051_fm_item_free(LH4051FMItem *item)
{
    if (!item)
        return;
    g_free(item->name);
    g_free(item->path);
    g_free(item->content_type);
    g_free(item);
}

LH4051FMItem *lh4051_fm_item_new(const char *path, const char *name)
{
    LH4051FMItem *item;
    GFile *file;
    GFileInfo *info;

    if (!path || !name)
        return NULL;

    item = g_new0(LH4051FMItem, 1);
    item->name = g_utf8_make_valid(name, -1);
    item->path = g_build_filename(path, name, NULL);
    item->type = G_FILE_TYPE_UNKNOWN;

    file = g_file_new_for_path(item->path);
    info = g_file_query_info(
        file,
        G_FILE_ATTRIBUTE_STANDARD_TYPE,
        G_FILE_QUERY_INFO_NONE,
        NULL,
        NULL
    );

    if (info) {
        item->type = g_file_info_get_file_type(info);
        g_object_unref(info);
    }

    g_object_unref(file);
    return item;
}

GPtrArray *lh4051_fm_list(const char *path)
{
    GPtrArray *items = g_ptr_array_new_with_free_func((GDestroyNotify)lh4051_fm_item_free);
    GDir *dir;

    if (!path || !*path)
        return items;

    dir = g_dir_open(path, 0, NULL);
    if (!dir)
        return items;

    for (const char *name = g_dir_read_name(dir);
         name != NULL;
         name = g_dir_read_name(dir)) {
        LH4051FMItem *item;

        if (name[0] == '.')
            continue;

        item = lh4051_fm_item_new(path, name);
        if (item)
            g_ptr_array_add(items, item);
    }

    g_dir_close(dir);
    g_ptr_array_sort_with_data(items, compare_items, NULL);
    return items;
}

void lh4051_fm_free_list(GPtrArray *items)
{
    if (items)
        g_ptr_array_unref(items);
}
