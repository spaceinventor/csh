#include <gtk/gtk.h>
#include <adwaita.h>

#include <slash/slash.h>

#include "csh_window.h"


static void
show_inspector (GSimpleAction *action,
                GVariant      *state,
                gpointer       user_data)
{
  gtk_window_set_interactive_debugging (TRUE);
}


static void
show_about (GSimpleAction *action,
            GVariant      *state,
            gpointer       user_data)
{
  GtkApplication *app = GTK_APPLICATION (user_data);
  GtkWindow *window = gtk_application_get_active_window (app);
  extern char *version_string;

  const char *authors[] = {
    "Johan de Claville Christiansen",
    NULL
  };


  gtk_show_about_dialog (window,
                         "program-name", "CSH (cubeshell)",
                         "title", "About CSH",
                         "comments", "Cubesat Shell",
                         "copyright", "Copyright © 2021 Space Inventor",
                         "version", version_string,
                         "logo-icon-name", "satellite-svgrepo-com",
                         "license-type", GTK_LICENSE_UNKNOWN,
                         "authors", authors,
                         "website", "https://github.com/spaceinventor/csh",
                         NULL);
}


static void show_window(GtkApplication * app) {
  CshWindow * window = csh_window_new(app);
  gtk_window_present(GTK_WINDOW(window));
}

static int cmd_ui(struct slash * slash) {
    static GActionEntry app_entries[] = {
    { "inspector", show_inspector, NULL, NULL, NULL },
    { "about", show_about, NULL, NULL, NULL },
  };
	g_autoptr(AdwApplication) app = adw_application_new("com.spaceinventor.csh", G_APPLICATION_FLAGS_NONE);
  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);





	g_signal_connect(app, "activate", G_CALLBACK(show_window), NULL);
	g_application_run(G_APPLICATION(app), slash->argc, slash->argv);
	return SLASH_SUCCESS;
}

slash_command(ui, cmd_ui, NULL, NULL);