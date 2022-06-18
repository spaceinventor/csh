#include "csh-page-whlctl.h"

#include "adwaita.h"

struct _CshPageWhlctl {
	AdwApplicationWindow parent_instance;
    GtkSpinButton * pidi;
    GtkSpinButton * pidp;
};

G_DEFINE_TYPE(CshPageWhlctl, csh_page_whlctl, ADW_TYPE_APPLICATION_WINDOW)

static void csh_page_whlctl_class_init(CshPageWhlctlClass * klass) {
	GtkWidgetClass * widget_class = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_template_from_resource(widget_class, "/com/spaceinventor/csh/csh-page-whlctl.ui");

    gtk_widget_class_bind_template_child(widget_class, CshPageWhlctl, pidi);
    gtk_widget_class_bind_template_child(widget_class, CshPageWhlctl, pidp);
}

static void csh_page_whlctl_init(CshPageWhlctl * self) {
	gtk_widget_init_template(GTK_WIDGET(self));
    printf("pidi %p pidp %p\n", self->pidi, self->pidp);
    gtk_spin_button_set_value(self->pidi, 0.02);
    gtk_spin_button_set_value(self->pidp, 0.04);
    printf("hest %p\n", self);
}

void csh_page_whlctl_update(CshPageWhlctl * self) {
    printf("hest %p\n", self);
}