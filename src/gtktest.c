#include <gtk/gtk.h>

#include <slash/slash.h>

#define EXAMPLE_APP_TYPE (example_app_get_type())
G_DECLARE_FINAL_TYPE(ExampleApp, example_app, EXAMPLE, APP, GtkApplication)

ExampleApp * example_app_new(void);

#define EXAMPLE_APP_WINDOW_TYPE (example_app_window_get_type())
G_DECLARE_FINAL_TYPE(ExampleAppWindow, example_app_window, EXAMPLE, APP_WINDOW, GtkApplicationWindow)

ExampleAppWindow * example_app_window_new(ExampleApp * app);
void example_app_window_open(ExampleAppWindow * win,
							 GFile * file);

struct _ExampleAppWindow {
	GtkApplicationWindow parent;

	GtkWidget * stack;
};

G_DEFINE_TYPE(ExampleAppWindow, example_app_window, GTK_TYPE_APPLICATION_WINDOW)

static void
example_app_window_init(ExampleAppWindow * win) {
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
example_app_window_class_init(ExampleAppWindowClass * class) {
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/com/spaceinventor/gtktest/gtktest.ui");
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ExampleAppWindow, stack);
}

ExampleAppWindow *
example_app_window_new(ExampleApp * app) {
	return g_object_new(EXAMPLE_APP_WINDOW_TYPE, "application", app, NULL);
}

void example_app_window_open(ExampleAppWindow * win,
							 GFile * file) {
	char * basename;
	GtkWidget *scrolled, *view;
	char * contents;
	gsize length;

	basename = g_file_get_basename(file);

	scrolled = gtk_scrolled_window_new();
	gtk_widget_set_hexpand(scrolled, TRUE);
	gtk_widget_set_vexpand(scrolled, TRUE);
	view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), view);
	gtk_stack_add_titled(GTK_STACK(win->stack), scrolled, basename, basename);

	if (g_file_load_contents(file, NULL, &contents, &length, NULL, NULL)) {
		GtkTextBuffer * buffer;

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
		gtk_text_buffer_set_text(buffer, contents, length);
		g_free(contents);
	}

	g_free(basename);
}

struct _ExampleApp {
	GtkApplication parent;
};

G_DEFINE_TYPE(ExampleApp, example_app, GTK_TYPE_APPLICATION);

static void
example_app_init(ExampleApp * app) {
}

static void
example_app_activate(GApplication * app) {
	ExampleAppWindow * win;

	win = example_app_window_new(EXAMPLE_APP(app));
	gtk_window_present(GTK_WINDOW(win));
}

static void
example_app_open(GApplication * app,
				 GFile ** files,
				 int n_files,
				 const char * hint) {
	GList * windows;
	ExampleAppWindow * win;
	int i;

	windows = gtk_application_get_windows(GTK_APPLICATION(app));
	if (windows)
		win = EXAMPLE_APP_WINDOW(windows->data);
	else
		win = example_app_window_new(EXAMPLE_APP(app));

	for (i = 0; i < n_files; i++)
		example_app_window_open(win, files[i]);

	gtk_window_present(GTK_WINDOW(win));
}

static void
example_app_class_init(ExampleAppClass * class) {
	G_APPLICATION_CLASS(class)->activate = example_app_activate;
	G_APPLICATION_CLASS(class)->open = example_app_open;
}

ExampleApp *
example_app_new(void) {
	return g_object_new(EXAMPLE_APP_TYPE,
						"application-id", "org.gtk.exampleapp",
						"flags", G_APPLICATION_HANDLES_OPEN,
						NULL);
}

static int cmd_gtk_test(struct slash * slash) {
	g_application_run(G_APPLICATION(example_app_new()), slash->argc, slash->argv);
	return SLASH_SUCCESS;
}

slash_command_sub(gtk, test, cmd_gtk_test, NULL, NULL);
