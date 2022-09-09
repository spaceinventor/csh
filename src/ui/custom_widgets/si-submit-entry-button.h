#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SUBMIT_ENTRY_BUTTON (si_submit_entry_button_get_type())

extern G_DECLARE_FINAL_TYPE(SiSubmitEntryButton, si_submit_entry_button, SI, SUBMIT_ENTRY_BUTTON, GtkWidget)

G_END_DECLS