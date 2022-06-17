#include <gtk/gtk.h>
#include <adwaita.h>

#include <slash/slash.h>

#include "csh-page-whlctl.h"

static void show_window(GtkApplication * app) {
    GtkWindow * window = g_object_new(csh_page_whlctl_get_type(), "application", app, NULL);
	gtk_window_present(window);
}

void * ui_task(void * param) {
	AdwApplication * app = adw_application_new("com.spaceinventor.csh", G_APPLICATION_FLAGS_NONE);
	g_signal_connect(app, "activate", G_CALLBACK(show_window), NULL);
	g_application_run(G_APPLICATION(app), 0, NULL);
	return NULL;
}


static int cmd_ui(struct slash * slash) {
	static pthread_t ui_handle;
	pthread_create(&ui_handle, NULL, &ui_task, NULL);
	return SLASH_SUCCESS;
}

slash_command(ui, cmd_ui, NULL, NULL);