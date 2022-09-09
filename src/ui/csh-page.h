#pragma once

#include <adwaita.h>

#define CSH_TYPE_PAGE (csh_page_get_type())

G_DECLARE_FINAL_TYPE (CshPage, csh_page, ADW, CSH_PAGE, AdwApplicationWindow)

AdwApplicationWindow *csh_page_new(void);
