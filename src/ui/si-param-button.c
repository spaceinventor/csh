#include "si-param-button.h"

#include <adwaita.h>

#include <param/param.h>
#include <param/param_list.h>
#include <param/param_string.h>
#include <param/param_client.h>

struct _SiParamButton {
	GtkWidget parent_instance;
	int param_id;
    int param_offset;
    int setvalue;
    char * label;
    GtkButton * button;
};

G_DEFINE_TYPE(SiParamButton, si_param_button, GTK_TYPE_WIDGET);

enum {
	PROP_0,
	PROP_PARAM_ID,
    PROP_PARAM_OFFSET,
    PROP_PARAM_SETVALUE,
    PROP_PARAM_LABEL,
	PROP_LAST_PROP,
};
static GParamSpec * props[PROP_LAST_PROP];


static void si_param_button_get_property(GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SiParamButton * self = SI_PARAM_BUTTON(object);
	switch (property_id) {
		case PROP_PARAM_ID:
			g_value_set_int(value, self->param_id);
			break;
        case PROP_PARAM_OFFSET:
			g_value_set_int(value, self->param_offset);
			break;
        case PROP_PARAM_SETVALUE:
			g_value_set_int(value, self->setvalue);
			break;
        case PROP_PARAM_LABEL:
            g_value_set_string(value, self->label);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
}

static void si_param_button_set_property(GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
    SiParamButton * self = SI_PARAM_BUTTON(object);
	switch (property_id) {
		case PROP_PARAM_ID:
			self->param_id = g_value_get_int(value);
			break;
        case PROP_PARAM_OFFSET:
			self->param_offset = g_value_get_int(value);
			break;
        case PROP_PARAM_SETVALUE:
			self->setvalue = g_value_get_int(value);
			break;
        case PROP_PARAM_LABEL:
            g_clear_pointer(&self->label, g_free);
            self->label = g_strdup(g_value_get_string(value));
			break;
    	default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
    g_object_notify_by_pspec (G_OBJECT (self), props[property_id]);
}

static void si_param_button_dispose (GObject *object) {
    SiParamButton * self = SI_PARAM_BUTTON(object);
    if (self->button)
        gtk_widget_unparent (GTK_WIDGET (self->button));
    G_OBJECT_CLASS(si_param_button_parent_class)->dispose(object);
}


static void clicked_cb(GtkWidget * button) {
    SiParamButton * self = SI_PARAM_BUTTON(gtk_widget_get_parent(button));
    if (self->param_id == 0)
        return;

    param_t * param = param_list_find_id(6, self->param_id);
    if (param == NULL)
        return;

    param_push_single(param, self->param_offset, &self->setvalue, 1, 6, 100, 2);
}


static void si_param_button_class_init(SiParamButtonClass * klass) {

    /* Properties */
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->set_property = si_param_button_set_property;
    object_class->get_property = si_param_button_get_property;
    object_class->dispose = si_param_button_dispose;
    props[PROP_PARAM_ID] = g_param_spec_int ("param_id", "Parameter id", "The id of the parameter", -1, INT_MAX, -1, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
    props[PROP_PARAM_OFFSET] = g_param_spec_int ("param_offset", "Parameter array offset", "Array index of the parameter", -1, INT_MAX, -1, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
    props[PROP_PARAM_SETVALUE] = g_param_spec_int ("setvalue", "Value to set", "Sets this integer value to parameter", -1, INT_MAX, -1, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
    props[PROP_PARAM_LABEL] = g_param_spec_string("label", "button label", "Button label", NULL, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
	
    GtkWidgetClass * widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
	gtk_widget_class_set_template_from_resource(widget_class, "/com/spaceinventor/csh/si-param-button.ui");
    gtk_widget_class_bind_template_child(widget_class, SiParamButton, button);
    gtk_widget_class_bind_template_callback(widget_class, clicked_cb);

}

static void si_param_button_init(SiParamButton * self) {
	gtk_widget_init_template(GTK_WIDGET(self));
}
