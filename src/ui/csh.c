#include <gtk/gtk.h>
#include <adwaita.h>

#include <slash/slash.h>

#include <param/param.h>
#include <param/param_client.h>
#include <csp/arch/csp_time.h>
#include <sys/types.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_queue.h>
#include "csh-page.h"
#include "FSS/fss.h"
#include "PDU/pdu.h"
#include <csp/csp_cmp.h>
#include <csp/csp.h>
#include <param/param_server.h>
#include <csp/csp_buffer.h>

static int8_t _testparam[16] = {0};
PARAM_DEFINE_STATIC_RAM(666, testparam, PARAM_TYPE_INT8, 16, sizeof(int8_t), PM_CONF, NULL, "", _testparam, "Test");


int node;
static pthread_t ui_handle;
static pthread_t data_handle;

static void activate (GtkApplication *app){
	GtkWindow *window;
	window = g_object_new(pdu_page_get_type(), "application", app, NULL);

	gtk_window_present(window);
}

void * data_task(void * param) {
	while(1) {
		param_pull_all(0, node, PM_TELEM, PM_REMOTE | PM_HWREG, 100, 2);
		usleep(100000);
	}
}

void * ui_task(void * param) {
	// Creates an adwaita application	
	AdwApplication * app;
	app = adw_application_new("com.spaceinventor.csh", G_APPLICATION_FLAGS_NONE);

	// Setting an on "activate" eventhandler to that adwaita application. 
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

	// Run the application.
	g_application_run(G_APPLICATION(app), 0, NULL);
	pthread_cancel(data_handle);
	return NULL;
}

static int cmd_ui(struct slash * slash) {
	// Find out which node the user entered the UI with and set it inside a global variable.
	// Return an error if no node was entered.
	if(slash->argc != 2){
		return SLASH_EUSAGE;
	}
	node = atoi(slash->argv[1]);

	char buffer[100] = "";
	struct csp_cmp_message msg;
    int res = csp_cmp_ident(node, 100, &msg);
    if(res != 0) return SLASH_EUSAGE;
    strncpy(buffer, msg.ident.hostname, 100);

	// Create two threads to handle the application.
	// UI thread. This will start the application and everything we put inside it.
	pthread_create(&ui_handle, NULL, &ui_task, NULL);

	// Data thread. This keeps pulling the params of the node, 
	// so that we always show the updated values.
	pthread_create(&data_handle, NULL, &data_task, NULL);
	// Slash command ran without errors, so return success.

	return SLASH_SUCCESS;
}

// Creates a command to use within a csh terminal. 
// In terminal:
// hostname: ui <node>
// hostname: ui 0 (This will open the UI with the node set to the host(yourself))
slash_command(ui, cmd_ui, "<node>", NULL);
