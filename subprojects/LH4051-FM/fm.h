#ifndef LH4051_FM_H
#define LH4051_FM_H
#include <gtk/gtk.h>
GPtrArray *lh4051_fm_list(const char *path);
void lh4051_fm_free_list(GPtrArray *items);
#endif
