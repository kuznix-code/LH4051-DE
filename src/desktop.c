#include "longhorn.h"
#include "LH4051-SESSION/session.h"

static GtkWidget *wallpaper_picture;
static GtkWidget *icons_box;
static GtkWidget *watermark_label;
static GtkWidget *selected_icon;
static GtkApplication *desktop_app;

/* The watermark sits in the bottom-right corner. In composite mode the sidebar is an
 * overlay drawn on top of the desktop, so the watermark is shifted left by the width the
 * sidebar currently covers. Opening is instant (so the watermark is never clipped);
 * closing slides it back to the corner. */
#define WATERMARK_MARGIN 14
#define WATERMARK_SLIDE_US (140 * 1000)
static int watermark_inset;           /* current extra right margin in px */
static int watermark_from, watermark_goal;
static gint64 watermark_start;
static guint watermark_tick_id;

typedef void (*IconAction)(GtkApplication *app);

static void act_documents(GtkApplication *a) { lh4051_explorer_open(a, lh4051_user_dir(G_USER_DIRECTORY_DOCUMENTS)); }
static void act_computer(GtkApplication *a)
{
    char *root = lh4051_computer_root();
    lh4051_explorer_open(a, root);
    g_free(root);
}
static void act_trash(GtkApplication *a) { (void)a; lh4051_open_recycle_bin(); }

static void icon_pressed(GtkGestureClick *g, int n, double x, double y, gpointer data)
{
    GtkWidget *icon = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g));
    IconAction action = (IconAction)data;
    (void)x; (void)y;
    if (selected_icon && selected_icon != icon) gtk_widget_remove_css_class(selected_icon, "selected");
    gtk_widget_add_css_class(icon, "selected");
    selected_icon = icon;
    if (n >= 2) action(desktop_app);
}

static GtkWidget *desktop_icon(const char *icon_name, const char *label, IconAction action)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *image = gtk_image_new_from_icon_name(icon_name);
    GtkWidget *text = gtk_label_new(label);
    GtkGesture *click = gtk_gesture_click_new();

    gtk_widget_add_css_class(box, "lh-desk-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(image), 48);
    gtk_widget_add_css_class(text, "lh-desk-icon-label");
    gtk_label_set_wrap(GTK_LABEL(text), TRUE);
    gtk_label_set_justify(GTK_LABEL(text), GTK_JUSTIFY_CENTER);
    gtk_label_set_max_width_chars(GTK_LABEL(text), 12);
    gtk_box_append(GTK_BOX(box), image);
    gtk_box_append(GTK_BOX(box), text);
    g_signal_connect(click, "pressed", G_CALLBACK(icon_pressed), (gpointer)action);
    gtk_widget_add_controller(box, GTK_EVENT_CONTROLLER(click));
    return box;
}

static void load_wallpaper(void)
{
    const char *custom = lh4051_prefs_wallpaper();
    GdkTexture *texture = NULL;
    GError *error = NULL;

    if (custom) {
        texture = gdk_texture_new_from_filename(custom, &error);
        if (!texture) {
            g_warning("LH4051 wallpaper: cannot load %s: %s", custom, error ? error->message : "?");
            g_clear_error(&error);
        }
    }
    if (!texture)
        texture = gdk_texture_new_from_resource(LH4051_RES "wallpaper.jpg");

    if (texture) {
        gtk_picture_set_paintable(GTK_PICTURE(wallpaper_picture), GDK_PAINTABLE(texture));
        gtk_widget_remove_css_class(wallpaper_picture, "lh-wallpaper-fallback");
        g_object_unref(texture);
    } else {
        gtk_picture_set_paintable(GTK_PICTURE(wallpaper_picture), NULL);
        gtk_widget_add_css_class(wallpaper_picture, "lh-wallpaper-fallback");
    }
}

static void watermark_apply(int inset)
{
    watermark_inset = inset;
    if (watermark_label) gtk_widget_set_margin_end(watermark_label, WATERMARK_MARGIN + inset);
}

static gboolean watermark_tick(GtkWidget *w, GdkFrameClock *clock, gpointer data)
{
    double t = (gdk_frame_clock_get_frame_time(clock) - watermark_start) / (double)WATERMARK_SLIDE_US;
    double inv;
    (void)w; (void)data;
    if (t >= 1.0) {
        watermark_apply(watermark_goal);
        watermark_tick_id = 0;
        return G_SOURCE_REMOVE;
    }
    inv = 1.0 - t;
    watermark_apply(watermark_from + (int)((watermark_goal - watermark_from) * (1.0 - inv * inv * inv)));
    return G_SOURCE_CONTINUE;
}

/* Tells the desktop how many pixels at its right edge are covered by the sidebar. */
void lh4051_desktop_set_sidebar_inset(int px, gboolean animate)
{
    if (!watermark_label) return;
    if (px < 0) px = 0;
    if (watermark_tick_id) {
        gtk_widget_remove_tick_callback(watermark_label, watermark_tick_id);
        watermark_tick_id = 0;
    }
    watermark_goal = px;
    if (!animate || px >= watermark_inset || !gtk_widget_get_mapped(watermark_label)) {
        watermark_apply(px);
        return;
    }
    watermark_from = watermark_inset;
    watermark_start = gdk_frame_clock_get_frame_time(gtk_widget_get_frame_clock(watermark_label));
    watermark_tick_id = gtk_widget_add_tick_callback(watermark_label, watermark_tick, NULL, NULL);
}

void lh4051_desktop_refresh(void)
{
    if (!wallpaper_picture) return;
    load_wallpaper();
    gtk_widget_set_visible(icons_box, lh4051_prefs_get(LH_PREF_DESKTOP_ICONS));
    gtk_widget_set_visible(watermark_label, lh4051_prefs_get(LH_PREF_WATERMARK));
}

static void on_desktop_prefs(gpointer data) { (void)data; lh4051_desktop_refresh(); }

static void on_desktop_destroy(GtkWidget *w, gpointer data)
{
    (void)w; (void)data;
    wallpaper_picture = icons_box = watermark_label = selected_icon = NULL;
    watermark_tick_id = 0;
    watermark_inset = 0;
}

GtkWidget *lh4051_desktop_new(GtkApplication *app)
{
    GtkWidget *root = gtk_overlay_new();
    char *mark;

    desktop_app = app;
    gtk_widget_add_css_class(root, "lh-desktop");
    gtk_widget_set_hexpand(root, TRUE);
    gtk_widget_set_vexpand(root, TRUE);

    wallpaper_picture = gtk_picture_new();
    gtk_picture_set_content_fit(GTK_PICTURE(wallpaper_picture), GTK_CONTENT_FIT_COVER);
    gtk_picture_set_can_shrink(GTK_PICTURE(wallpaper_picture), TRUE);
    gtk_widget_set_hexpand(wallpaper_picture, TRUE);
    gtk_widget_set_vexpand(wallpaper_picture, TRUE);
    gtk_overlay_set_child(GTK_OVERLAY(root), wallpaper_picture);

    icons_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_halign(icons_box, GTK_ALIGN_START);
    gtk_widget_set_valign(icons_box, GTK_ALIGN_START);
    gtk_widget_set_margin_top(icons_box, 14);
    gtk_widget_set_margin_start(icons_box, 12);
    gtk_box_append(GTK_BOX(icons_box), desktop_icon("folder-documents", "My Documents", act_documents));
    gtk_box_append(GTK_BOX(icons_box), desktop_icon("computer", "My Computer", act_computer));
    gtk_box_append(GTK_BOX(icons_box), desktop_icon("user-trash", "Recycle Bin", act_trash));
    gtk_overlay_add_overlay(GTK_OVERLAY(root), icons_box);

    mark = g_strdup_printf("Windows Code Name \"Longhorn\"\nFor testing purposes only. Build 4051 · LH4051-DE %s",
                           LH4051_VERSION);
    watermark_label = gtk_label_new(mark);
    g_free(mark);
    gtk_widget_add_css_class(watermark_label, "lh-watermark");
    gtk_label_set_justify(GTK_LABEL(watermark_label), GTK_JUSTIFY_RIGHT);
    gtk_widget_set_halign(watermark_label, GTK_ALIGN_END);
    gtk_widget_set_valign(watermark_label, GTK_ALIGN_END);
    gtk_widget_set_margin_end(watermark_label, WATERMARK_MARGIN);
    gtk_widget_set_margin_bottom(watermark_label, LH4051_PANEL_HEIGHT + 10);
    gtk_widget_set_can_target(watermark_label, FALSE);
    gtk_overlay_add_overlay(GTK_OVERLAY(root), watermark_label);

    g_signal_connect(root, "destroy", G_CALLBACK(on_desktop_destroy), NULL);
    lh4051_prefs_watch(on_desktop_prefs, NULL);
    lh4051_desktop_refresh();
    return root;
}
