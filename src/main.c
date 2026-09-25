#include "longhorn.h"
#include "LH4051-SESSION.h"

void load_shell_css(void) {
    GtkCssProvider *provider=gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        "window { font-family: Sans; }"
        ".lh-desktop { background:#4f86ad; }"
        ".lh-wallpaper { background:linear-gradient(180deg,#70b4d7 0%,#9bd1e5 45%,#d8c88b 72%,#8d9849 100%); }"
        ".lh-watermark { color:rgba(255,255,255,.78); font-size:12px; padding:6px 10px; }"
        ".lh-panel { background:linear-gradient(180deg,#59646e 0%,#323b44 48%,#202830 100%); color:#fff; }"
        ".lh-task { min-height:34px; margin:4px 2px; padding:0 10px; border-radius:4px; border:1px solid rgba(255,255,255,.24); background:linear-gradient(#66727d,#303942); color:white; }"
        ".lh-start { min-width:86px; min-height:34px; margin:4px 5px 4px 6px; border-radius:5px; border:1px solid #b7c7d2; background:linear-gradient(#6f8798,#263642); color:white; font-weight:bold; }"
        ".lh-tray { padding:0 9px; border-left:1px solid rgba(255,255,255,.16); }"
        ".lh-clock { font-size:12px; font-weight:600; min-width:72px; }"
        ".lh-sidebar { background:rgba(28,35,42,.90); color:white; border-left:1px solid rgba(255,255,255,.30); }"
        ".lh-tile { margin:8px; padding:12px; border-radius:5px; background:rgba(70,82,94,.70); border:1px solid rgba(255,255,255,.15); }"
        ".lh-tile-title { font-weight:bold; color:#dce7ef; } .lh-digital { font-size:18px; font-weight:600; }"
        ".lh-start-menu { background:#e9e5d8; border:1px solid #87909a; } .lh-start-left { background:linear-gradient(180deg,#5d7385,#263541); color:white; }"
        ".lh-start-header { padding:12px; font-weight:bold; font-size:16px; } .lh-start-item { min-height:42px; margin:2px 8px; padding:5px 8px; border-radius:3px; } .lh-start-right { color:#26313a; padding:10px; }"
        ".lh-explorer { background:#f5f5f5; } .lh-titlebar { background:linear-gradient(#dfe5e9,#9ba8b1); color:#17212a; } .lh-close { background:linear-gradient(#ef665e,#b52c28); color:white; min-width:42px; }"
        ".lh-toolbar { background:#e2e6e8; border-bottom:1px solid #aab2b7; } .lh-phode { background:linear-gradient(90deg,#3982bd,#8bbce0); color:white; padding:14px; } .lh-filter { background:#e8ecef; border-right:1px solid #bdc5ca; } .lh-menu { background:#f0f0f0; }"
    );
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),GTK_STYLE_PROVIDER(provider),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static void on_activate(GtkApplication *app,gpointer user_data){
    (void)user_data;
    g_print("Starting LH4051-DE %s session...\n",LH4051_VERSION);
    load_shell_css();
    lh4051_session_start(app);
}
static void on_shutdown(GApplication *app,gpointer user_data){
    (void)app; (void)user_data;
    lh4051_session_stop();
}
int main(int argc,char*argv[]){
    GtkApplication*app=gtk_application_new("org.kuznix.lh4051de",G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app,"activate",G_CALLBACK(on_activate),NULL);
    g_signal_connect(app,"shutdown",G_CALLBACK(on_shutdown),NULL);
    int status=g_application_run(G_APPLICATION(app),argc,argv);
    g_object_unref(app);
    return status;
}
