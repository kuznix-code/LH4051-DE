#include "fm.h"
GPtrArray *lh4051_fm_list(const char*path){GPtrArray*a=g_ptr_array_new_with_free_func(g_free);GDir*d=g_dir_open(path,0,NULL);if(!d)return a;const char*n;while((n=g_dir_read_name(d))){if(n[0]=='.')continue;g_ptr_array_add(a,g_strdup(n));}g_dir_close(d);return a;}
void lh4051_fm_free_list(GPtrArray*items){if(items)g_ptr_array_unref(items);}
