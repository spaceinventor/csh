#include <stdio.h>
#include <stdlib.h>

#include <param/param.h>
#include "csh_internals.h"

uint32_t _serial0;


#define PARAMID_SERIAL0                      31

PARAM_DEFINE_STATIC_RAM(PARAMID_SERIAL0, serial0, PARAM_TYPE_INT32, -1, 0, PM_HWREG, NULL, "", &_serial0, NULL);

void serial_init(void) {
    _serial0 = rand();
}

uint32_t serial_get(void) {
    return _serial0;
}