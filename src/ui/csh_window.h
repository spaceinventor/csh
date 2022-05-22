#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define ADW_TYPE_CSH_WINDOW (csh_window_get_type())

G_DECLARE_FINAL_TYPE (CshWindow, csh_window, ADW, CSH_WINDOW, AdwApplicationWindow)

CshWindow *csh_window_new (GtkApplication *application);

G_END_DECLS
