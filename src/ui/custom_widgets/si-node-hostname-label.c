#include "si-node-hostname-label.h"

#include <adwaita.h>

#include <param/param.h>
#include <param/param_list.h>
#include <param/param_string.h>
#include <param/param_client.h>
#include <csp/csp_cmp.h>

// Destructor functions.
static void si_node_hostname_label_dispose (GObject *object);
static void si_node_hostname_label_finalize(GObject *object);

// Dette er "klassen" som vi laver. Den fortæller hvad denne widget indeholder.
// Denne widget skal kun indeholde en label med text, men man kunne sagtens lave flere widgets her.
// parent_instance, boiler plate, den skal bare være der.
// Alt derefter kan være flere forskellige widgets.
struct _SiNodeHostnameLabel {
	GtkWidget parent_instance;
    GtkWidget * label;
};

G_DEFINE_TYPE(SiNodeHostnameLabel, si_node_hostname_label, GTK_TYPE_WIDGET);

static int get_si_node_hostname(char *buffer) {
    struct csp_cmp_message msg;
    extern int node;
    int res = csp_cmp_ident(node, 100, &msg);
    if(res != 0) return -1;
    strncpy(buffer, msg.ident.hostname, 100);
    int cmp_res = strcmp(buffer, "");
    return cmp_res;
}

static void si_node_hostname_label_init(SiNodeHostnameLabel * self) {
    GtkWidget *widget = GTK_WIDGET(self);

    char buffer[100] = "";
    int res = get_si_node_hostname(buffer);
    if(res > 0){
        self->label = gtk_label_new(buffer);
    }

    gtk_widget_set_parent(self->label, widget);
}

static void si_node_hostname_label_dispose(GObject *object){
    SiNodeHostnameLabel *self = SI_NODE_HOSTNAME_LABEL(object);
    g_clear_pointer(&self->label, gtk_widget_unparent);
    G_OBJECT_CLASS(si_node_hostname_label_parent_class)->dispose(object);
}

static void si_node_hostname_label_finalize(GObject *object){
    G_OBJECT_CLASS(si_node_hostname_label_parent_class)->finalize(object);
}

static void si_node_hostname_label_class_init(SiNodeHostnameLabelClass * klass) {
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = si_node_hostname_label_dispose;
    object_class->finalize = si_node_hostname_label_finalize;
	
    GtkWidgetClass * widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);


	gtk_widget_class_set_template_from_resource(widget_class, "/com/spaceinventor/csh/custom_widgets/si-node-hostname-label.ui");
    gtk_widget_class_bind_template_child(widget_class, SiNodeHostnameLabel, label);

}

GtkWidget * si_node_hostname_label_new(void){
    return g_object_new(SI_NODE_TYPE_HOSTNAME_LABEL, NULL);
}

