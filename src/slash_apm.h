#pragma once

#include "walkdir.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef int (*libmain_t)(void);
typedef void (*libinfo_t)(void);
typedef struct apm_entry_s apm_entry_t;
extern void apm_queue_add(apm_entry_t * e);
struct apm_entry_s {
    void * handle;

    char path[WALKDIR_MAX_PATH_SIZE];
    const char * file;

    char args[256];
    libmain_t libmain_f;
    libinfo_t libinfo_f;
    int apm_init_version;
    apm_entry_t * next;
};

#ifdef __cplusplus
}
#endif
