#include "LH4051-SESSION.h"
#include "LH4051-WM.h"
#include "longhorn.h"
static GtkApplication *session_app;
void lh4051_session_start(GtkApplication *app){
    session_app=app;
    lh4051_wm_init();
    create_desktop(app);
    create_longhorn_panel(app);
    create_longhorn_sidebar(app);
}
void lh4051_session_stop(void){lh4051_wm_shutdown();session_app=NULL;}
GtkApplication *lh4051_session_app(void){return session_app;}
