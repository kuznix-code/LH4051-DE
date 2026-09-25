#ifndef LONGHORN_H
#define LONGHORN_H
#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#define LH4051_VERSION "0.0.0"
#define LH4051_PANEL_HEIGHT 42
#define LH4051_SIDEBAR_WIDTH 280
void load_shell_css(void);
void create_desktop(GtkApplication *app);
void create_longhorn_panel(GtkApplication *app);
void create_longhorn_sidebar(GtkApplication *app);
void create_start_menu(GtkApplication *app);
void create_file_explorer(GtkApplication *app, const char *path);\nvoid create_lh4051_settings(GtkApplication *app);\nvoid create_lh4051_appstore(GtkApplication *app);
void longhorn_show_start_menu(GtkApplication *app);
void longhorn_hide_start_menu(void);
void longhorn_toggle_start_menu(GtkApplication *app);
#endif
