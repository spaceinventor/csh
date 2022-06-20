#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define PARAM_BUTTON (si_param_button_get_type())

extern G_DECLARE_FINAL_TYPE(SiParamButton, si_param_button, SI, PARAM_BUTTON, GtkWidget)

G_END_DECLS