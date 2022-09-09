#include "si-submit-entry-button.h"

#include <adwaita.h>

#include <param/param.h>
#include <param/param_list.h>
#include <param/param_string.h>
#include <param/param_client.h>

#include <csp/csp_cmp.h>
#include <csp/csp.h>
#include <param/param_server.h>
#include <csp/csp_buffer.h>

#include <ctype.h>
#include "../csh.h"

struct _SiSubmitEntryButton {
	GtkWidget parent_instance;
    int param_id;
    GtkBox * box;
    GtkEntry * entry;
    GtkButton * button;
};

G_DEFINE_TYPE(SiSubmitEntryButton, si_submit_entry_button, GTK_TYPE_WIDGET);

enum {
	PROP_0,
	PROP_PARAM_ID,
	PROP_LAST_PROP,
};
static GParamSpec * props[PROP_LAST_PROP];

static void si_submit_entry_button_get_property(GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SiSubmitEntryButton * self = SI_SUBMIT_ENTRY_BUTTON(object);
	switch (property_id) {
		case PROP_PARAM_ID:
			g_value_set_int(value, self->param_id);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
}

static void si_submit_entry_button_set_property(GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
    SiSubmitEntryButton * self = SI_SUBMIT_ENTRY_BUTTON(object);
	switch (property_id) {
		case PROP_PARAM_ID:
			self->param_id = g_value_get_int(value);
			break;
    	default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
			break;
	}
    g_object_notify_by_pspec (G_OBJECT (self), props[property_id]);
}

static void si_submit_entry_button_dispose (GObject *object) {
    SiSubmitEntryButton * self = SI_SUBMIT_ENTRY_BUTTON(object);
    if (self->button)
        gtk_widget_unparent (GTK_WIDGET (self->button));
    if (self->entry)
        gtk_widget_unparent (GTK_WIDGET (self->entry));
    if (self->box)
        gtk_widget_unparent (GTK_WIDGET (self->box));
    G_OBJECT_CLASS(si_submit_entry_button_parent_class)->dispose(object);
}


static void clicked_cb(GtkWidget * button) {
    // SiSubmitEntryButton * self = SI_SUBMIT_ENTRY_BUTTON(gtk_widget_get_parent(gtk_widget_get_parent(button)));
    // if(self->param_id == 0){
    //     return;
    // }
    
    // GtkEntryBuffer *entry_buffer = gtk_entry_get_buffer(self->entry);
    // const char *buff = gtk_entry_buffer_get_text(entry_buffer);
    // float floatbuff = strtof(buff, NULL);

    // struct queueParam *newParam = NULL;
    // newParam = newQueueParam(self->param_id, floatbuff);
}

void insert_text_handler (GtkEditable *editable, const gchar *text, gint length, gint *position, gpointer data){
    // SiSubmitEntryButton * self = SI_SUBMIT_ENTRY_BUTTON(gtk_widget_get_parent(GTK_WIDGET(editable)));
    int i;

    for (i = 0; i < length; i++) {
        if (!isdigit(text[i]) && text[i] != '.') {
            g_signal_stop_emission_by_name(G_OBJECT(editable), "insert-text");
            return;
        }
    }
}


static void si_submit_entry_button_class_init(SiSubmitEntryButtonClass * klass) {
    /* Properties */
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->set_property = si_submit_entry_button_set_property;
    object_class->get_property = si_submit_entry_button_get_property;
    object_class->dispose = si_submit_entry_button_dispose;
    props[PROP_PARAM_ID] = g_param_spec_int ("param_id", "Parameter id", "The id of the parameter", -1, INT_MAX, -1, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
    g_object_class_install_properties (object_class, PROP_LAST_PROP, props);

    GtkWidgetClass * widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
	gtk_widget_class_set_template_from_resource(widget_class, "/com/spaceinventor/csh/custom_widgets/si-submit-entry-button.ui");
    gtk_widget_class_bind_template_child(widget_class, SiSubmitEntryButton, box);
    gtk_widget_class_bind_template_child(widget_class, SiSubmitEntryButton, entry);
    gtk_widget_class_bind_template_child(widget_class, SiSubmitEntryButton, button);
    gtk_widget_class_bind_template_callback(widget_class, clicked_cb);

}


static void si_submit_entry_button_init(SiSubmitEntryButton * self) {
	gtk_widget_init_template(GTK_WIDGET(self));
    g_signal_connect(gtk_editable_get_delegate(GTK_EDITABLE(self->entry)), "insert-text", G_CALLBACK(insert_text_handler), NULL);
}
