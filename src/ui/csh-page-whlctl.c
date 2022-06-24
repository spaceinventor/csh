#include "csh-page-whlctl.h"
#include "si-param-label.h"
#include "si-param-button.h"

#include <param/param.h>
#include <param/param_client.h>

#include <adwaita.h>

struct _CshPageWhlctl {
	AdwApplicationWindow parent_instance;
};

G_DEFINE_TYPE(CshPageWhlctl, csh_page_whlctl, ADW_TYPE_APPLICATION_WINDOW)

static void csh_page_whlctl_class_init(CshPageWhlctlClass * klass) {
	GtkWidgetClass * widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(widget_class, "/com/spaceinventor/csh/csh-page-whlctl.ui");
}



static void csh_page_whlctl_init(CshPageWhlctl * self) {
    g_type_ensure(PARAM_LABEL);
    g_type_ensure(PARAM_BUTTON);
	gtk_widget_init_template(GTK_WIDGET(self));
}
