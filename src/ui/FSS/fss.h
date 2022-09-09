#pragma once

#include <adwaita.h>

#define FSS_TYPE_PAGE (fss_page_get_type())

G_DECLARE_FINAL_TYPE (FssPage, fss_page, ADW, FSS_PAGE, AdwApplicationWindow)

AdwApplicationWindow *fss_page_new(void);
