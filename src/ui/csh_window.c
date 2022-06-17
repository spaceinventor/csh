#include <gtk/gtk.h>
#include <adwaita.h>

#include "csh_window.h"

#include "pages/welcome/csh-page-welcome.h"
#include "pages/whlctl/csh-page-whlctl.h"

struct _CshWindow {
	AdwApplicationWindow parent_instance;

	GtkWidget * color_scheme_button;
	AdwLeaflet * main_leaflet;
	AdwLeaflet * subpage_leaflet;
};

G_DEFINE_TYPE(CshWindow, csh_window, ADW_TYPE_APPLICATION_WINDOW)

static void notify_visible_child_cb(CshWindow * self) {
	adw_leaflet_navigate(self->main_leaflet, ADW_NAVIGATION_DIRECTION_FORWARD);
}

static void back_clicked_cb(CshWindow * self) {
	adw_leaflet_navigate(self->main_leaflet, ADW_NAVIGATION_DIRECTION_BACK);
}

static void leaflet_back_clicked_cb(CshWindow * self) {
	adw_leaflet_navigate(self->subpage_leaflet, ADW_NAVIGATION_DIRECTION_BACK);
}

static void leaflet_next_page_cb(CshWindow * self) {
	adw_leaflet_navigate(self->subpage_leaflet, ADW_NAVIGATION_DIRECTION_FORWARD);
}

static void csh_window_class_init(CshWindowClass * klass) {
	printf("Class init\n");
	GtkWidgetClass * widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_q, GDK_CONTROL_MASK, "window.close", NULL);

	gtk_widget_class_set_template_from_resource(widget_class, "/com/spaceinventor/csh/csh_window.ui");
	gtk_widget_class_bind_template_child(widget_class, CshWindow, main_leaflet);
	gtk_widget_class_bind_template_child(widget_class, CshWindow, subpage_leaflet);

	gtk_widget_class_bind_template_callback(widget_class, notify_visible_child_cb);
	gtk_widget_class_bind_template_callback(widget_class, back_clicked_cb);
	gtk_widget_class_bind_template_callback(widget_class, leaflet_back_clicked_cb);
	gtk_widget_class_bind_template_callback(widget_class, leaflet_next_page_cb);
}

static void csh_window_init(CshWindow * self) {
	AdwStyleManager * manager = adw_style_manager_get_default();

	g_type_ensure(CSH_PAGE_WELCOME);
    g_type_ensure(CSH_PAGE_WHLCTL);

	printf("Window init\n");
	gtk_widget_init_template(GTK_WIDGET(self));

	adw_leaflet_navigate(self->main_leaflet, ADW_NAVIGATION_DIRECTION_FORWARD);
}

CshWindow * csh_window_new(GtkApplication * application) {
	printf("Window new\n");
	return g_object_new(ADW_TYPE_CSH_WINDOW, "application", application, NULL);
}
