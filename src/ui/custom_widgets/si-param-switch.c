#include "si-param-switch.h"

#include <adwaita.h>

#include <param/param.h>
#include <param/param_list.h>
#include <param/param_string.h>
#include <param/param_client.h>

struct _SiParamSwitch {
	GtkWidget parent_instance;
	int param_id;
    int param_offset;
    GtkSwitch * si_switch;
};

G_DEFINE_TYPE(SiParamSwitch, si_param_switch, GTK_TYPE_WIDGET);

enum {
	PROP_0,
	PROP_PARAM_ID,
    PROP_PARAM_OFFSET,
	PROP_LAST_PROP,
};
static GParamSpec * props[PROP_LAST_PROP];

extern int node;

static void si_param_switch_get_property(GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SiParamSwitch * self = SI_PARAM_SWITCH(object);
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


static void si_param_switch_set_property(GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
    SiParamSwitch * self = SI_PARAM_SWITCH(object);
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
    g_object_notify_by_pspec (G_OBJECT (self), props[property_id]);
}


static void si_param_switch_dispose (GObject *object) {
    if (object == NULL)
        return;
    SiParamSwitch * self = SI_PARAM_SWITCH(object);
    if (self->si_switch){
        gtk_widget_unparent (GTK_WIDGET (self->si_switch));
        self->si_switch = NULL;
    }
    G_OBJECT_CLASS(si_param_switch_parent_class)->dispose(object);
}


static void switch_active(GtkWidget * si_switch){
    GtkSwitch *sw = (GtkSwitch *)si_switch;
    SiParamSwitch * self = SI_PARAM_SWITCH(gtk_widget_get_parent(si_switch));
    gboolean current_active = gtk_switch_get_active(sw);

    gtk_switch_set_state(sw, current_active);

    param_t * param = param_list_find_id(node, self->param_id);
    if (param == NULL)
        return;

    param_push_single(param, self->param_offset, &current_active, 1, node, 100, 2);
}


static void si_param_switch_class_init(SiParamSwitchClass * klass) {
    /* Properties */
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->set_property = si_param_switch_set_property;
    object_class->get_property = si_param_switch_get_property;
    object_class->dispose = si_param_switch_dispose;
    props[PROP_PARAM_ID] = g_param_spec_int ("param_id", "Parameter id", "The id of the parameter", -1, INT_MAX, -1, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
    props[PROP_PARAM_OFFSET] = g_param_spec_int ("param_offset", "Parameter array offset", "Array index of the parameter", -1, INT_MAX, -1, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
    g_object_class_install_properties (object_class, PROP_LAST_PROP, props);
	
    GtkWidgetClass * widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
	gtk_widget_class_set_template_from_resource(widget_class, "/com/spaceinventor/csh/custom_widgets/si-param-switch.ui");
    gtk_widget_class_bind_template_child(widget_class, SiParamSwitch, si_switch);
    gtk_widget_class_bind_template_callback(widget_class, switch_active);
    printf("CLASS INIT\n");
}

static int switch_param_handler(void * object) {
    if (object == NULL)
        return 0;

    SiParamSwitch * self = (SiParamSwitch *)object;

    if (self->param_id == 0)
        return 1;

    param_t * param = param_list_find_id(node, self->param_id);
    if (param == NULL)
        return 1;

    int8_t offset_value = param_get_int8_array(param, self->param_offset);
    
    gtk_switch_set_state(self->si_switch, offset_value);

    return 1;
}

static void si_param_switch_init(SiParamSwitch * self) {
	gtk_widget_init_template(GTK_WIDGET(self));
    g_timeout_add(100, switch_param_handler, self);
}
