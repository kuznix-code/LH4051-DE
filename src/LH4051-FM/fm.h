#ifndef LH4051_FM_H
#define LH4051_FM_H

#include <gio/gio.h>
#include <gtk/gtk.h>

typedef struct {
    char *name;
    char *path;
    GFileType type;
    goffset size;
    char *content_type;
    gint64 modified;
} LH4051FMItem;

GPtrArray *lh4051_fm_list(const char *path);
void lh4051_fm_free_list(GPtrArray *items);
LH4051FMItem *lh4051_fm_item_new(const char *path, const char *name);
void lh4051_fm_item_free(LH4051FMItem *item);

#endif
