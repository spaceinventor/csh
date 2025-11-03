#pragma once

#include <slash/slash.h>
#ifdef __cplusplus
extern "C"
#endif

/**
 * @brief Use environment to attempt completion of the the given "token" 
 * @param slash pointer to a slash object
 * @param token the command line to complete
 */
extern void env_var_ref_completer(struct slash * slash, char * token);

#ifdef __cplusplus
}
#endif
