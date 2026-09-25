#include "longhorn.h"
#include "LH4051-WM/wm.h"
#include "LH4051-SESSION/session.h"
static GtkWidget *clock_label;
static GtkWidget *task_box;

static gboolean update_clock(gpointer d){
    (void)d;
    if(!clock_label)return G_SOURCE_REMOVE;
    GDateTime*n=g_date_time_new_now_local();
    char*s=g_date_time_format(n,"%I:%M %p");
    gtk_label_set_text(GTK_LABEL(clock_label),s);
    g_free(s); g_date_time_unref(n);
    return G_SOURCE_CONTINUE;
}
static void task_clicked(GtkButton*b,gpointer d){
    (void)b;
    lh4051_wm_activate(GTK_WINDOW(d));
}
static void rebuild_tasks(void *data){
    (void)data;
    if(!task_box)return;
    GtkWidget *child=gtk_widget_get_first_child(task_box);
    while(child){GtkWidget *next=gtk_widget_get_next_sibling(child);gtk_box_remove(GTK_BOX(task_box),child);child=next;}
    for(guint i=0;i<lh4051_wm_count();i++){
        GtkWindow*w=lh4051_wm_get_window(i);
        const char*t=lh4051_wm_get_title(i);
        GtkWidget*b=gtk_button_new_with_label(t?t:"LH4051 Window");
        gtk_widget_add_css_class(b,"lh-task");
        g_signal_connect(b,"clicked",G_CALLBACK(task_clicked),w);
        gtk_box_append(GTK_BOX(task_box),b);
    }
}
static void start_clicked(GtkButton*b,gpointer d){(void)b;longhorn_toggle_start_menu(GTK_APPLICATION(d));}
static void explorer_clicked(GtkButton*b,gpointer d){(void)b;create_file_explorer(GTK_APPLICATION(d),g_get_home_dir());}

void create_longhorn_panel(GtkApplication*app){
    GtkWidget*w=gtk_application_window_new(app); GtkWindow*win=GTK_WINDOW(w);
    lh4051_session_setup_shell_window(win,LH4051_SHELL_PANEL);
    gtk_widget_set_size_request(w,1,LH4051_PANEL_HEIGHT);
    GtkWidget*bar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0); gtk_widget_add_css_class(bar,"lh-panel"); gtk_window_set_child(win,bar);
    GtkWidget*start=gtk_button_new_with_label("⊞  start"); gtk_widget_add_css_class(start,"lh-start");
    g_signal_connect(start,"clicked",G_CALLBACK(start_clicked),app); gtk_box_append(GTK_BOX(bar),start);
    task_box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0); gtk_widget_set_hexpand(task_box,TRUE); gtk_box_append(GTK_BOX(bar),task_box);
    GtkWidget*quick=gtk_button_new_with_label("▣"); gtk_widget_add_css_class(quick,"lh-task");
    g_signal_connect(quick,"clicked",G_CALLBACK(explorer_clicked),app); gtk_box_append(GTK_BOX(bar),quick);
    GtkWidget*tray=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,2); gtk_widget_add_css_class(tray,"lh-tray");
    gtk_box_append(GTK_BOX(tray),gtk_button_new_with_label("‹")); gtk_box_append(GTK_BOX(tray),gtk_label_new("🔊")); gtk_box_append(GTK_BOX(tray),gtk_label_new("⌁"));
    clock_label=gtk_label_new("--:-- --"); gtk_widget_add_css_class(clock_label,"lh-clock"); gtk_box_append(GTK_BOX(tray),clock_label); gtk_box_append(GTK_BOX(bar),tray);
    lh4051_wm_set_changed_callback(rebuild_tasks,NULL);
    rebuild_tasks(NULL); update_clock(NULL); g_timeout_add_seconds(1,update_clock,NULL);
    gtk_window_present(win);
}
