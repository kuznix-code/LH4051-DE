#ifndef LH4051_FM_H
#define LH4051_FM_H

#include <gio/gio.h>

typedef struct {
    char *name;          /* file name on disk */
    char *display_name;  /* UTF-8 name for the UI */
    char *path;          /* full path */
    GFileType type;
    goffset size;
    char *type_desc;     /* human readable type ("JPEG image") */
    gint64 modified;     /* unix seconds, 0 if unknown */
    GIcon *icon;
} LH4051FMItem;

typedef enum { LH4051_FM_SORT_NAME, LH4051_FM_SORT_SIZE, LH4051_FM_SORT_TYPE, LH4051_FM_SORT_DATE } LH4051FMSort;

/* Lists a directory. Returns NULL and sets *error on failure. `filter` is a
 * case-insensitive substring match on the name (may be NULL). */
GPtrArray *lh4051_fm_list(const char *path, gboolean show_hidden, const char *filter,
                          LH4051FMSort sort, gboolean ascending, GError **error);
void lh4051_fm_free_list(GPtrArray *items);
char *lh4051_fm_format_size(const LH4051FMItem *item); /* "" for folders */
char *lh4051_fm_format_date(gint64 unix_seconds);

#endif
