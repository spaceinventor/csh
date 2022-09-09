#include "fss.h"
#include "../custom_widgets/si-submit-entry-button.h"

#include <param/param_client.h>
#include <param/param.h>
#include <csp/arch/csp_time.h>
#include <sys/types.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_queue.h>

#include <adwaita.h>
#include "../csh.h"

#include <csp/csp_cmp.h>
#include <csp/csp.h>
#include <param/param_server.h>
#include <csp/csp_buffer.h>

static void fss_page_dispose (GObject *object);
static void fss_page_finalize (GObject *object);

struct queueParam *first = NULL;
struct queueParam *added = NULL;

void CleanSetParamMemory(struct queueParam *listElement){
    struct queueParam *next;
    while(listElement){
        next = listElement->nextInLine;
        free(listElement);
        listElement = next;
    }
}

struct queueParam *newQueueParam(const int param_id, const float value){
    struct queueParam *param = NULL;
    param = malloc(sizeof(struct queueParam));
    if(param == NULL){
        return param;
    }
    param->previousInLine = added;
    param->nextInLine = NULL;
	if(first == NULL){
		first = param;
		if(first != NULL){
			added = param;	
		}
	}
	else{
		added->nextInLine = param;
		if(added->nextInLine != NULL){
			added->nextInLine->previousInLine = added;
			added = added->nextInLine;
		}
	}
    added->param_id = param_id;
    added->value = value;
    return added;
}

struct _FssPage {
    AdwApplicationWindow parent_instance;
};

G_DEFINE_TYPE(FssPage, fss_page, ADW_TYPE_APPLICATION_WINDOW)

static void fss_page_init(FssPage * self){
    GtkWidget *widget = GTK_WIDGET(self);
    g_type_ensure(SUBMIT_ENTRY_BUTTON);
    gtk_widget_init_template(widget);
}

static void fss_page_dispose(GObject *object){
    G_OBJECT_CLASS(fss_page_parent_class)->dispose(object);
}

static void fss_page_finalize(GObject *object){
    G_OBJECT_CLASS(fss_page_parent_class)->finalize(object);
}

void dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data){
    if(response_id == GTK_RESPONSE_OK){
        printf("RESPONDED WITH OK\n");
        extern int node;

        param_queue_t queue;
        csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
        if(packet == NULL){
            return;
        }
        packet->data[0] = PARAM_PUSH_REQUEST_V2;
        packet->data[1] = 0;

        param_queue_init(&queue, &packet->data[2], PARAM_SERVER_MTU - 2, 0, PARAM_QUEUE_TYPE_SET, 2);

        struct queueParam *step = NULL;
        step = first;
        while(step){
            int val = (int)(step->value);

            param_t *param = param_list_find_id(node, step->param_id);
            param_queue_add(&queue, param, 0, &val);
            step = step->nextInLine;
        }
        if(first){
            int result = param_push_queue(&queue, 1, node, 100, 0);

            CleanSetParamMemory(first);

            printf("%d\n", result);
        }

    }
    else if(response_id == GTK_RESPONSE_CANCEL){
        printf("RESPONDED WITH CANCEL\n");
    }
    else{
        printf("RESPONDED WITH SOMETHING ELSE\n");
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
}

void dialog_box (GtkWidget * button){
    GtkWindow *ancestor = GTK_WINDOW(gtk_widget_get_ancestor(button, GTK_TYPE_WINDOW));

    GtkWidget *dialog, *label, *content_area;

    // Create the widgets
    dialog = gtk_dialog_new_with_buttons ("Current Queue", ancestor, GTK_DIALOG_MODAL, "_Send", GTK_RESPONSE_OK, "_Close", GTK_RESPONSE_CANCEL, NULL);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), ancestor);
    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    label = gtk_label_new ("message");

    // Ensure that the dialog box is destroyed when the user responds

    g_signal_connect_swapped(dialog, "response", G_CALLBACK(dialog_response), dialog);
    // Add the label, and show everything we’ve added

    gtk_box_append(GTK_BOX(content_area), label);
    gtk_widget_show (dialog);
}

static void fss_page_class_init(FssPageClass * klass){
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = fss_page_dispose;
    object_class->finalize = fss_page_finalize;

    GtkWidgetClass * widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(widget_class, "/com/spaceinventor/csh/FSS/fss.ui");
    gtk_widget_class_bind_template_callback(widget_class, dialog_box);
}

AdwApplicationWindow * fss_page_new(void){
    return g_object_new(FSS_TYPE_PAGE, NULL);
}