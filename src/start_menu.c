#include "longhorn.h"
#ifdef G_OS_UNIX
#include <gio/gdesktopappinfo.h>
#endif

static GtkWidget *start_window;

static void hide_start(GtkButton *b, gpointer d){(void)b;(void)d;longhorn_hide_start_menu();}
static void open_home(GtkButton *b,gpointer d){(void)b;create_file_explorer(GTK_APPLICATION(d),g_get_home_dir());longhorn_hide_start_menu();}
static void open_settings(GtkButton *b,gpointer d){(void)b;create_lh4051_settings(GTK_APPLICATION(d));longhorn_hide_start_menu();}
static void open_store(GtkButton *b,gpointer d){(void)b;create_lh4051_appstore(GTK_APPLICATION(d));longhorn_hide_start_menu();}

#ifdef G_OS_UNIX
static void launch_app(GtkButton *b,gpointer d){
    const char *id=g_object_get_data(G_OBJECT(b),"lh-desktop-id");
    GDesktopAppInfo *info=id?g_desktop_app_info_new(id):NULL;
    GError *error=NULL;
    (void)d;
    if(info){
        if(!g_app_info_launch(G_APP_INFO(info),NULL,NULL,&error))
            g_warning("LH4051 app launch failed for %s: %s",id,error?error->message:"unknown error");
        g_clear_error(&error);
        g_object_unref(info);
    }
    longhorn_hide_start_menu();
}
#endif

static GtkWidget *start_button(const char *label,GCallback cb,gpointer data){
    GtkWidget *b=gtk_button_new_with_label(label);
    gtk_widget_add_css_class(b,"lh-start-item");
    g_signal_connect(b,"clicked",cb,data);
    return b;
}

#ifdef G_OS_UNIX
static void populate_apps(GtkWidget *box){
    GList *apps=g_app_info_get_all();
    for(GList *l=apps;l;l=l->next){
        GAppInfo *info=l->data;
        if(!g_app_info_should_show(info)) continue;
        const char *name=g_app_info_get_display_name(info);
        const char *id=g_app_info_get_id(info);
        if(!name||!id) continue;
        GtkWidget *b=start_button(name,G_CALLBACK(launch_app),NULL);
        g_object_set_data_full(G_OBJECT(b),"lh-desktop-id",g_strdup(id),g_free);
        gtk_box_append(GTK_BOX(box),b);
    }
    g_list_free_full(apps,g_object_unref);
}
#endif

void longhorn_hide_start_menu(void){if(start_window)gtk_widget_set_visible(start_window,FALSE);}
void longhorn_show_start_menu(GtkApplication *a){if(!start_window)create_start_menu(a);else gtk_window_present(GTK_WINDOW(start_window));}
void longhorn_toggle_start_menu(GtkApplication *a){if(!start_window){create_start_menu(a);return;}if(gtk_widget_get_visible(start_window))longhorn_hide_start_menu();else gtk_window_present(GTK_WINDOW(start_window));}

void create_start_menu(GtkApplication *app){
    GtkWidget *w=gtk_application_window_new(app),*root,*left,*right,*scroll,*apps;
    GtkWindow *win=GTK_WINDOW(w); start_window=w;
    gtk_layer_init_for_window(win); gtk_layer_set_layer(win,GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(win,GTK_LAYER_SHELL_EDGE_BOTTOM,TRUE); gtk_layer_set_anchor(win,GTK_LAYER_SHELL_EDGE_LEFT,TRUE);
    gtk_layer_set_margin(win,GTK_LAYER_SHELL_EDGE_BOTTOM,LH4051_PANEL_HEIGHT);
    gtk_layer_set_keyboard_mode(win,GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
    gtk_layer_set_namespace(win,"lh4051-start-menu");
    gtk_widget_set_visible(w,TRUE);
    gtk_window_set_default_size(win,760,540);
    root=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0); gtk_widget_add_css_class(root,"lh-start-menu"); gtk_window_set_child(win,root);

    left=gtk_box_new(GTK_ORIENTATION_VERTICAL,4); gtk_widget_set_size_request(left,340,-1); gtk_widget_add_css_class(left,"lh-start-left");
    GtkWidget *header=gtk_label_new("◉  LH4051"); gtk_widget_add_css_class(header,"lh-start-header"); gtk_label_set_xalign(GTK_LABEL(header),0); gtk_box_append(GTK_BOX(left),header);
    GtkWidget *search=gtk_search_entry_new(); gtk_widget_add_css_class(search,"lh-start-search"); gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(search),"Search apps"); gtk_box_append(GTK_BOX(left),search);
    gtk_box_append(GTK_BOX(left),start_button("📁  My Documents",G_CALLBACK(open_home),app));
    gtk_box_append(GTK_BOX(left),start_button("⚙  LH4051 Settings",G_CALLBACK(open_settings),app));
    gtk_box_append(GTK_BOX(left),start_button("🛍  LH4051 App Store",G_CALLBACK(open_store),app));
    GtkWidget *section=gtk_label_new("All Programs"); gtk_widget_add_css_class(section,"lh-start-section"); gtk_label_set_xalign(GTK_LABEL(section),0); gtk_box_append(GTK_BOX(left),section);
    scroll=gtk_scrolled_window_new(); gtk_widget_set_vexpand(scroll,TRUE); apps=gtk_box_new(GTK_ORIENTATION_VERTICAL,1); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),apps); gtk_box_append(GTK_BOX(left),scroll);
#ifdef G_OS_UNIX
    populate_apps(apps);
#endif

    right=gtk_box_new(GTK_ORIENTATION_VERTICAL,5); gtk_widget_add_css_class(right,"lh-start-right");
    GtkWidget *p=gtk_label_new("Places and system"); gtk_label_set_xalign(GTK_LABEL(p),0); gtk_box_append(GTK_BOX(right),p);
    gtk_box_append(GTK_BOX(right),start_button("📄  Documents",G_CALLBACK(open_home),app));
    gtk_box_append(GTK_BOX(right),start_button("⚙  Settings",G_CALLBACK(open_settings),app));
    gtk_box_append(GTK_BOX(right),start_button("🛍  App Store",G_CALLBACK(open_store),app));
    GtkWidget *sp=gtk_box_new(GTK_ORIENTATION_VERTICAL,0); gtk_widget_set_vexpand(sp,TRUE); gtk_box_append(GTK_BOX(right),sp);
    gtk_box_append(GTK_BOX(right),start_button("Close Start Menu",G_CALLBACK(hide_start),NULL));
    gtk_box_append(GTK_BOX(root),left); gtk_box_append(GTK_BOX(root),right); gtk_window_present(win);
}