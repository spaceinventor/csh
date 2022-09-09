#include "csh-page.h"
// #include "custom_widgets/si-param-label.h"
// #include "custom_widgets/si-param-button.h"
// #include "custom_widgets/si-param-switch.h"
#include "custom_widgets/si-submit-entry-button.h"


#include <param/param.h>
#include <param/param_client.h>

#include <adwaita.h>

static void csh_page_dispose (GObject *object);
static void csh_page_finalize (GObject *object);

struct _CshPage {
    AdwApplicationWindow parent_instance;
};

G_DEFINE_TYPE(CshPage, csh_page, ADW_TYPE_APPLICATION_WINDOW)

static void csh_page_init(CshPage * self){
    GtkWidget *widget = GTK_WIDGET(self);
    g_type_ensure(SUBMIT_ENTRY_BUTTON);
    gtk_widget_init_template(widget);

}

static void csh_page_dispose(GObject *object){
    G_OBJECT_CLASS(csh_page_parent_class)->dispose(object);
}

static void csh_page_finalize(GObject *object){
    G_OBJECT_CLASS(csh_page_parent_class)->finalize(object);
}

static void csh_page_class_init(CshPageClass * klass){
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = csh_page_dispose;
    object_class->finalize = csh_page_finalize;

    GtkWidgetClass * widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(widget_class, "/com/spaceinventor/csh/csh-page.ui");
}

AdwApplicationWindow * csh_page_new(void){
    return g_object_new(CSH_TYPE_PAGE, NULL);
}