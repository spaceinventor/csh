#include <gtk/gtk.h>
#include <adwaita.h>

#include <slash/slash.h>

#include <param/param.h>
#include <param/param_client.h>

#include "csh-page-whlctl.h"

static AdwApplication * app;
static GtkWindow * window;

static void show_window(GtkApplication * app) {
    window = g_object_new(csh_page_whlctl_get_type(), "application", app, NULL);
	gtk_window_present(window);
}

void * ui_task(void * param) {
	app = adw_application_new("com.spaceinventor.csh", G_APPLICATION_FLAGS_NONE);
	g_signal_connect(app, "activate", G_CALLBACK(show_window), NULL);
	g_application_run(G_APPLICATION(app), 0, NULL);
	return NULL;
}

void * data_task(void * param) {
	while(1) {
		param_pull_all(0, 6, PM_TELEM, PM_REMOTE | PM_HWREG, 100, 2);
		usleep(100000);
	}
}

static int cmd_ui(struct slash * slash) {
	static pthread_t ui_handle;
	pthread_create(&ui_handle, NULL, &ui_task, NULL);
	static pthread_t data_handle;
	pthread_create(&data_handle, NULL, &data_task, NULL);
	return SLASH_SUCCESS;
}

slash_command(ui, cmd_ui, NULL, NULL);