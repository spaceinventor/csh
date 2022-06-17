#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CSH_PAGE_WHLCTL (csh_page_whlctl_get_type())

G_DECLARE_FINAL_TYPE (CshPageWhlctl, csh_page_whlctl, ADW, CSH_PAGE_WHLCTL, AdwBin)

G_END_DECLS
