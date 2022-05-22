#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CSH_PAGE_WELCOME (csh_page_welcome_get_type())

G_DECLARE_FINAL_TYPE (CshPageWelcome, csh_page_welcome, ADW, CSH_PAGE_WELCOME, AdwBin)

G_END_DECLS
