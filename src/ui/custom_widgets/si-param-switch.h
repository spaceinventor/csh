#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define PARAM_SWITCH (si_param_switch_get_type())

extern G_DECLARE_FINAL_TYPE(SiParamSwitch, si_param_switch, SI, PARAM_SWITCH, GtkWidget)

G_END_DECLS