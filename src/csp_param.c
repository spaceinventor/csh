#include <stdint.h>
#include <stddef.h>

#include <param/param.h>
#include <vmem/vmem.h>


extern vmem_t vmem_csp;

#define PARAMID_CSP_RTABLE					 12

const char * csp_rtable_docstr = "";
const PARAM_DEFINE_STATIC_VMEM(PARAMID_CSP_RTABLE,      csp_rtable,        PARAM_TYPE_STRING, 64, 0, PM_SYSCONF, NULL, "", csp, 0, csp_rtable_docstr);


