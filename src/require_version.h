#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <ctype.h>
#include <string.h>
#include <stdbool.h>


typedef struct {
    int major;
    int minor;
    int patch;
} version_t;


bool parse_version(const char *version_str, version_t *version_out, bool allow_suffix);

bool compare_version(const version_t *version, const char *constraint, bool verbose);

#ifdef __cplusplus
}
#endif
