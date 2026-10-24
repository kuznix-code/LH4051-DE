#include "fm.h"
#include <string.h>

#define FM_ATTRS \
    "standard::name,standard::display-name,standard::type,standard::size," \
    "standard::content-type,standard::icon,standard::is-hidden,standard::is-backup," \
    "time::modified"

static void item_free(LH4051FMItem *item)
{
    if (!item) return;
    g_free(item->name);
    g_free(item->display_name);
    g_free(item->path);
    g_free(item->type_desc);
    g_clear_object(&item->icon);
    g_free(item);
}

typedef struct { LH4051FMSort sort; gboolean asc; } SortCtx;

static gint compare_items(gconstpointer a, gconstpointer b, gpointer user_data)
{
    const LH4051FMItem *ia = *(LH4051FMItem *const *)a;
    const LH4051FMItem *ib = *(LH4051FMItem *const *)b;
    const SortCtx *ctx = user_data;
    gboolean da = ia->type == G_FILE_TYPE_DIRECTORY, db = ib->type == G_FILE_TYPE_DIRECTORY;
    int r = 0;

    if (da != db) return da ? -1 : 1; /* folders always first */

    switch (ctx->sort) {
    case LH4051_FM_SORT_SIZE: r = (ia->size > ib->size) - (ia->size < ib->size); break;
    case LH4051_FM_SORT_DATE: r = (ia->modified > ib->modified) - (ia->modified < ib->modified); break;
    case LH4051_FM_SORT_TYPE: r = g_utf8_collate(ia->type_desc ? ia->type_desc : "", ib->type_desc ? ib->type_desc : ""); break;
    default: break;
    }
    if (r == 0) {
        char *ka = g_utf8_casefold(ia->display_name, -1), *kb = g_utf8_casefold(ib->display_name, -1);
        r = g_utf8_collate(ka, kb);
        g_free(ka); g_free(kb);
    }
    return ctx->asc ? r : -r;
}

GPtrArray *lh4051_fm_list(const char *path, gboolean show_hidden, const char *filter,
                          LH4051FMSort sort, gboolean ascending, GError **error)
{
    GFile *dir;
    GFileEnumerator *en;
    GPtrArray *items;
    GFileInfo *info;
    char *needle = (filter && *filter) ? g_utf8_casefold(filter, -1) : NULL;
    SortCtx ctx = {sort, ascending};

    if (!path || !*path) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "No folder specified.");
        g_free(needle);
        return NULL;
    }

    dir = g_file_new_for_path(path);
    en = g_file_enumerate_children(dir, FM_ATTRS, G_FILE_QUERY_INFO_NONE, NULL, error);
    if (!en) { g_object_unref(dir); g_free(needle); return NULL; }

    items = g_ptr_array_new_with_free_func((GDestroyNotify)item_free);
    while ((info = g_file_enumerator_next_file(en, NULL, NULL)) != NULL) {
        const char *name = g_file_info_get_name(info);
        const char *display = g_file_info_get_display_name(info);
        const char *ct = g_file_info_get_content_type(info);
        LH4051FMItem *item;
        GDateTime *mtime;

        if (!show_hidden && (g_file_info_get_is_hidden(info) || g_file_info_get_is_backup(info))) {
            g_object_unref(info);
            continue;
        }
        if (needle) {
            char *folded = g_utf8_casefold(display ? display : name, -1);
            gboolean match = strstr(folded, needle) != NULL;
            g_free(folded);
            if (!match) { g_object_unref(info); continue; }
        }

        item = g_new0(LH4051FMItem, 1);
        item->name = g_strdup(name);
        item->display_name = g_utf8_make_valid(display ? display : name, -1);
        item->path = g_build_filename(path, name, NULL);
        item->type = g_file_info_get_file_type(info);
        item->size = g_file_info_get_size(info);
        if (item->type == G_FILE_TYPE_DIRECTORY)
            item->type_desc = g_strdup("File Folder");
        else
            item->type_desc = ct ? g_content_type_get_description(ct) : g_strdup("File");
        item->icon = g_file_info_get_icon(info) ? g_object_ref(g_file_info_get_icon(info)) : NULL;
        mtime = g_file_info_get_modification_date_time(info);
        if (mtime) { item->modified = g_date_time_to_unix(mtime); g_date_time_unref(mtime); }
        g_ptr_array_add(items, item);
        g_object_unref(info);
    }

    g_file_enumerator_close(en, NULL, NULL);
    g_object_unref(en);
    g_object_unref(dir);
    g_free(needle);
    g_ptr_array_sort_with_data(items, compare_items, &ctx);
    return items;
}

void lh4051_fm_free_list(GPtrArray *items)
{
    if (items) g_ptr_array_unref(items);
}

char *lh4051_fm_format_size(const LH4051FMItem *item)
{
    if (!item || item->type == G_FILE_TYPE_DIRECTORY) return g_strdup("");
    return g_format_size(item->size);
}

char *lh4051_fm_format_date(gint64 t)
{
    GDateTime *dt;
    char *s;
    if (t <= 0) return g_strdup("");
    dt = g_date_time_new_from_unix_local(t);
    s = g_date_time_format(dt, "%-m/%-d/%Y %-I:%M %p");
    g_date_time_unref(dt);
    return s;
}
