#include "csh-page-whlctl.h"

struct _CshPageWhlctl {
	AdwBin parent_instance;
};

G_DEFINE_TYPE(CshPageWhlctl, csh_page_whlctl, ADW_TYPE_BIN)

static void csh_page_whlctl_class_init(CshPageWhlctlClass * klass) {
	GtkWidgetClass * widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(widget_class, "/com/spaceinventor/csh/pages/welcome/csh-page-whlctl.ui");
}

static void csh_page_whlctl_init(CshPageWhlctl * self) {
	gtk_widget_init_template(GTK_WIDGET(self));
}
