#include "longhorn.h"
static void close_settings(GtkButton*b,gpointer d){(void)b;gtk_window_destroy(GTK_WINDOW(d));}
void create_lh4051_settings(GtkApplication *app){
    GtkWidget *w=gtk_application_window_new(app),*root,*scroll,*box;
    gtk_window_set_title(GTK_WINDOW(w),"LH4051 Settings"); gtk_window_set_default_size(GTK_WINDOW(w),760,560);
    root=gtk_box_new(GTK_ORIENTATION_VERTICAL,0); gtk_widget_add_css_class(root,"lh-settings"); gtk_window_set_child(GTK_WINDOW(w),root);
    GtkWidget *bar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,4); gtk_widget_add_css_class(bar,"lh-titlebar");
    GtkWidget *title=gtk_label_new("LH4051 Settings"); gtk_widget_set_hexpand(title,TRUE); gtk_label_set_xalign(GTK_LABEL(title),0); gtk_box_append(GTK_BOX(bar),title);
    GtkWidget *close=gtk_button_new_with_label("✕"); g_signal_connect(close,"clicked",G_CALLBACK(close_settings),w); gtk_box_append(GTK_BOX(bar),close); gtk_box_append(GTK_BOX(root),bar);
    scroll=gtk_scrolled_window_new(); gtk_widget_set_vexpand(scroll,TRUE); box=gtk_box_new(GTK_ORIENTATION_VERTICAL,0); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),box); gtk_box_append(GTK_BOX(root),scroll);
    const char *sections[]={"Appearance","Desktop","Taskbar & Start","File Manager","Applications","System"};
    for(guint i=0;i<G_N_ELEMENTS(sections);i++){GtkWidget *card=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);gtk_widget_add_css_class(card,"lh-settings-card");GtkWidget *h=gtk_label_new(sections[i]);gtk_widget_add_css_class(h,"lh-start-section");gtk_label_set_xalign(GTK_LABEL(h),0);gtk_box_append(GTK_BOX(card),h);GtkWidget *c=gtk_check_button_new_with_label(i==0?"Use LH4051 visual theme":"Enable component");gtk_check_button_set_active(GTK_CHECK_BUTTON(c),TRUE);gtk_box_append(GTK_BOX(card),c);gtk_box_append(GTK_BOX(box),card);}
    gtk_window_present(GTK_WINDOW(w));
}