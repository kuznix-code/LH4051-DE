#include "longhorn.h"
static void close_store(GtkButton*b,gpointer d){(void)b;gtk_window_destroy(GTK_WINDOW(d));}
static void search_pk(GtkButton*b,gpointer d){
    GtkWidget *entry=g_object_get_data(G_OBJECT(b),"entry"),*status=g_object_get_data(G_OBJECT(b),"status");
    const char *q=gtk_editable_get_text(GTK_EDITABLE(entry));
    if(!q||!*q){gtk_label_set_text(GTK_LABEL(status),"Enter a package name.");return;}
    gchar *argv[]={"pkcon","search","name",(gchar*)q,NULL}; GError *e=NULL;
    GSubprocess *p=g_subprocess_newv((const gchar * const*)argv,G_SUBPROCESS_FLAGS_STDOUT_PIPE|G_SUBPROCESS_FLAGS_STDERR_PIPE,&e);
    if(!p){gtk_label_set_text(GTK_LABEL(status),e?e->message:"PackageKit is unavailable.");g_clear_error(&e);return;}
    gchar *out=NULL,*err=NULL;
    if(g_subprocess_communicate_utf8(p,NULL,NULL,&out,&err,&e)) gtk_label_set_text(GTK_LABEL(status),(out&&*out)?out:((err&&*err)?err:"No results."));
    else {gtk_label_set_text(GTK_LABEL(status),e?e->message:"PackageKit search failed.");g_clear_error(&e);}
    g_free(out);g_free(err);g_object_unref(p);
}
void create_lh4051_appstore(GtkApplication *app){
    GtkWidget *w=gtk_application_window_new(app),*root;
    gtk_window_set_title(GTK_WINDOW(w),"LH4051 App Store"); gtk_window_set_default_size(GTK_WINDOW(w),820,620);
    root=gtk_box_new(GTK_ORIENTATION_VERTICAL,6);gtk_widget_add_css_class(root,"lh-store");gtk_window_set_child(GTK_WINDOW(w),root);
    GtkWidget *bar=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,4);gtk_widget_add_css_class(bar,"lh-titlebar");GtkWidget *title=gtk_label_new("LH4051 App Store · PackageKit");gtk_widget_set_hexpand(title,TRUE);gtk_label_set_xalign(GTK_LABEL(title),0);gtk_box_append(GTK_BOX(bar),title);GtkWidget *close=gtk_button_new_with_label("✕");g_signal_connect(close,"clicked",G_CALLBACK(close_store),w);gtk_box_append(GTK_BOX(bar),close);gtk_box_append(GTK_BOX(root),bar);
    GtkWidget *entry=gtk_search_entry_new();gtk_entry_set_placeholder_text(GTK_ENTRY(entry),"Search packages");gtk_widget_set_margin_start(entry,10);gtk_widget_set_margin_end(entry,10);gtk_box_append(GTK_BOX(root),entry);
    GtkWidget *button=gtk_button_new_with_label("Search PackageKit");GtkWidget *status=gtk_label_new("PackageKit backend: search packages from configured repositories.");gtk_label_set_wrap(GTK_LABEL(status),TRUE);gtk_label_set_xalign(GTK_LABEL(status),0);gtk_widget_add_css_class(status,"lh-store-card");g_object_set_data(G_OBJECT(button),"entry",entry);g_object_set_data(G_OBJECT(button),"status",status);g_signal_connect(button,"clicked",G_CALLBACK(search_pk),NULL);gtk_box_append(GTK_BOX(root),button);gtk_box_append(GTK_BOX(root),status);
    gtk_window_present(GTK_WINDOW(w));
}