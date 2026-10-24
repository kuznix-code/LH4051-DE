#include "longhorn.h"
#include "LH4051-SESSION/session.h"

static gboolean opt_windowed;

static void on_activate(GApplication *app, gpointer user_data)
{
    (void)user_data;
    if (lh4051_session_app()) return; /* shell already running */
    lh4051_prefs_load();
    lh4051_theme_load();
    lh4051_session_start(GTK_APPLICATION(app), opt_windowed);
}

static void on_shutdown(GApplication *app, gpointer user_data)
{
    (void)app; (void)user_data;
    lh4051_session_stop();
}

static gint on_local_options(GApplication *app, GVariantDict *options, gpointer user_data)
{
    (void)app; (void)user_data;
    if (g_variant_dict_contains(options, "version")) {
        g_print("LH4051-DE %s\n", LH4051_VERSION);
        return 0;
    }
    opt_windowed = g_variant_dict_contains(options, "windowed");
    return -1; /* continue normal startup */
}

int main(int argc, char *argv[])
{
    GtkApplication *app = gtk_application_new("org.kuznix.lh4051de", G_APPLICATION_DEFAULT_FLAGS);
    int status;

    g_application_add_main_option(G_APPLICATION(app), "windowed", 'w', G_OPTION_FLAG_NONE,
                                  G_OPTION_ARG_NONE, "Run the shell in a normal window instead of full screen", NULL);
    g_application_add_main_option(G_APPLICATION(app), "version", 'v', G_OPTION_FLAG_NONE,
                                  G_OPTION_ARG_NONE, "Print the version and exit", NULL);
    g_signal_connect(app, "handle-local-options", G_CALLBACK(on_local_options), NULL);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(on_shutdown), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
