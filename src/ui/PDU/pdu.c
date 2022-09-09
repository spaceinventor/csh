#include "pdu.h"

#include <param/param.h>
#include <param/param_client.h>

#include <adwaita.h>
#include "../csh.h"

#include "../custom_widgets/si-param-switch.h"

static void pdu_page_dispose (GObject *object);
static void pdu_page_finalize (GObject *object);

struct _PduPage {
    AdwApplicationWindow parent_instance;
};

G_DEFINE_TYPE(PduPage, pdu_page, ADW_TYPE_APPLICATION_WINDOW)

static void pdu_page_init(PduPage * self){
    GtkWidget *widget = GTK_WIDGET(self);
    g_type_ensure(PARAM_SWITCH);
    gtk_widget_init_template(widget);
}

static void pdu_page_dispose(GObject *object){
    G_OBJECT_CLASS(pdu_page_parent_class)->dispose(object);
}

static void pdu_page_finalize(GObject *object){
    G_OBJECT_CLASS(pdu_page_parent_class)->finalize(object);
}

static void pdu_page_class_init(PduPageClass * klass){
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = pdu_page_dispose;
    object_class->finalize = pdu_page_finalize;

    GtkWidgetClass * widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(widget_class, "/com/spaceinventor/csh/PDU/pdu.ui");
}

AdwApplicationWindow * pdu_page_new(void){
    return g_object_new(PDU_TYPE_PAGE, NULL);
}