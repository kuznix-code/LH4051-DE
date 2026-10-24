#include "longhorn.h"

/* The stylesheet lives in data/theme.css and is embedded as a GResource, so a
 * single binary carries the whole look (and MSVC's string-literal size limit
 * never applies). */
void lh4051_theme_load(void)
{
    GtkCssProvider *provider = gtk_css_provider_new();
    GtkSettings *settings = gtk_settings_get_default();

    gtk_css_provider_load_from_resource(provider, LH4051_RES "theme.css");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    if (settings)
        g_object_set(settings, "gtk-application-prefer-dark-theme", FALSE, NULL);
}
