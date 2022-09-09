#include "si-param-label.h"

#include <adwaita.h>

#include <param/param.h>
#include <param/param_list.h>
#include <param/param_string.h>
#include <param/param_client.h>

struct _SiParamLabel {
	GtkWidget parent_instance;
	int param_id;
    int param_offset;
    GtkLabel * label;
};

G_DEFINE_TYPE(SiParamLabel, si_param_label, GTK_TYPE_WIDGET);

enum {
	PROP_0,
	PROP_PARAM_ID,
    PROP_PARAM_OFFSET,
	PROP_LAST_PROP,
};
static GParamSpec * props[PROP_LAST_PROP];


static void si_param_label_get_property(GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SiParamLabel * self = SI_PARAM_LABEL(object);
	switch (property_id) {
		case PROP_PARAM_ID:
			g_value_set_int(value, self->param_id);
			break;
        case PROP_PARAM_OFFSET:
			g_value_set_int(value, self->param_offset);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
}

static void si_param_label_set_property(GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
    SiParamLabel * self = SI_PARAM_LABEL(object);
	switch (property_id) {
		case PROP_PARAM_ID:
			self->param_id = g_value_get_int(value);
			break;
        case PROP_PARAM_OFFSET:
			self->param_offset = g_value_get_int(value);
			break;
    	default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
}

static void si_param_label_dispose (GObject *object) {
    if (object == NULL)
        return;
    SiParamLabel * self = SI_PARAM_LABEL(object);
    if (self->label) {
        gtk_widget_unparent(GTK_WIDGET(self->label));
        self->label = NULL;
    }
    G_OBJECT_CLASS (si_param_label_parent_class)->dispose(object);
}


static void si_param_label_class_init(SiParamLabelClass * klass) {

    /* Properties */
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->set_property = si_param_label_set_property;
    object_class->get_property = si_param_label_get_property;
    object_class->dispose = si_param_label_dispose;
    props[PROP_PARAM_ID] = g_param_spec_int ("param_id", "Parameter id", "The id of the parameter", -1, INT_MAX, -1, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
    props[PROP_PARAM_OFFSET] = g_param_spec_int ("param_offset", "Parameter array offset", "Array index of the parameter", -1, INT_MAX, -1, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
    g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
	
    GtkWidgetClass * widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
	gtk_widget_class_set_template_from_resource(widget_class, "/com/spaceinventor/csh/custom_widgets/si-param-label.ui");
    gtk_widget_class_bind_template_child(widget_class, SiParamLabel, label);

}

static int time_handler(void * object) {

    if (object == NULL)
        return 0;

    SiParamLabel * self = SI_PARAM_LABEL(object);

    if (self->label == 0) {
        return 0;
    }

    if (self->param_id == 0)
        return 1;

    param_t * param = param_list_find_id(6, self->param_id);
    if (param == NULL)
        return 1;

    char buf[100];
    param_value_str(param, self->param_offset, buf, 100);
    
    gtk_label_set_text(self->label, buf);

    return 1;
}

static void si_param_label_init(SiParamLabel * self) {
	gtk_widget_init_template(GTK_WIDGET(self));
    g_timeout_add(100, time_handler, self);
    
}
