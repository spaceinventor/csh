#pragma once

#include <adwaita.h>

#define PDU_TYPE_PAGE (pdu_page_get_type())

G_DECLARE_FINAL_TYPE (PduPage, pdu_page, ADW, PDU_PAGE, AdwApplicationWindow)

AdwApplicationWindow *pdu_page_new(void);
