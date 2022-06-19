#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define PARAM_LABEL (si_param_label_get_type())

extern G_DECLARE_FINAL_TYPE(SiParamLabel, si_param_label, SI, PARAM_LABEL, GtkWidget)

G_END_DECLS