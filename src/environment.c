#include <sys/queue.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "environment.h"

static SLIST_HEAD( csh_env_llist, csh_env_entry ) csh_env = SLIST_HEAD_INITIALIZER(csh_env);
struct csh_env_entry {
    char *name;
    char *value;
    SLIST_ENTRY(csh_env_entry) list;
};

static struct csh_env_entry *csh_get_env_entry(const char *name) {
    struct csh_env_entry *cur;
    SLIST_FOREACH(cur, &csh_env, list) {
        if(strcmp(name, cur->name) == 0) {
            return cur;
        }
    }
    return NULL;
}

static char *csh_copy_char_and_grow(char *dest, size_t at, size_t *dest_len, char c) {
    if (at == *dest_len) {
        *dest_len += 16;
        dest = realloc(dest, *dest_len);
    }
    dest[at] = c;
    return dest;
}

void csh_foreach_var(csh_foreach_var_cb cb, void *ctx) {
    struct csh_env_entry *cur;
    SLIST_FOREACH(cur, &csh_env, list) {
        cb(cur->name, ctx);
    }
}

char *csh_getvar(const char *name) {
    char *result = NULL;
    struct csh_env_entry *cur = csh_get_env_entry(name);
    if (NULL != cur) {
        result = cur->value;
    }
    return result;
}

int csh_putvar(const char *name, const char *value) {
    struct csh_env_entry *existing = csh_get_env_entry(name);
    if(NULL != existing) {
        free(existing->value);
        existing->value = strdup(value);
    } else {
        struct csh_env_entry *new_entry = calloc(1, sizeof(*new_entry));
        new_entry->name = strdup(name);
        new_entry->value = strdup(value);
        SLIST_INSERT_HEAD(&csh_env, new_entry, list);
    }
    return 0;
}

int csh_delvar(const char *name) {
    int res = 0;
    struct csh_env_entry *var = csh_get_env_entry(name);
    if(var) {
        free(var->name);
        free(var->value);
        SLIST_REMOVE(&csh_env, var, csh_env_entry, list);
        free(var);
    } else {
        res = 1;
    }
    return res;
}

void csh_clearenv() {
    struct csh_env_entry *var;
    while (!SLIST_EMPTY(&csh_env)) {
        var = SLIST_FIRST(&csh_env);
        free(var->name);
        free(var->value);
        SLIST_REMOVE(&csh_env, var, csh_env_entry, list);
        free(var);
    }
}

char *csh_expand_vars(const char *input) {
    size_t input_len = strlen(input);
    size_t res_len = input_len;
    char *res = strdup(input);
    size_t idx = 0;
    size_t res_idx = 0;
    size_t var_s = 0;
    size_t var_e = 0;
    bool in_var = false;
    while (idx < input_len) {
        if(!in_var) {
            if(idx < input_len - 2) {
                if(input[idx] == '$' && input[idx+1] == '(') {
                    in_var = true;
                    var_s = idx + 2;
                } else {
                    res = csh_copy_char_and_grow(res, res_idx, &res_len, input[idx]);
                    res_idx++;
                }
            } else {
                res = csh_copy_char_and_grow(res, res_idx, &res_len, input[idx]);
                res_idx++;
            }            
        } else {
            if(input[idx] == ')') {
                char *var_value;
                var_e = idx;
                in_var = false;
                char var_name[(var_e - var_s) + 1];
                strncpy(var_name, &input[var_s], (var_e - var_s));
                var_name[var_e - var_s] = 0;
                var_value = csh_getvar(var_name);
                if(var_value) {
                    for(int i = 0; i < strlen(var_value); i++) {
                        res = csh_copy_char_and_grow(res, res_idx, &res_len, var_value[i]);
                        res_idx++;
                    }
                } else {
                    /* Variable not found in environment, will be skipped entirely */
                }
                var_s = var_e;
            }
        }
        idx++;
    }
    res[res_idx] = 0;
    return res;
}

#if 0
/* 
Small test code to try things out, for example:
- compile: gcc -Wall -g -o environment_test environment.c 
- valgrind --leak-check=full -s ./environment_test
*/
int main(int argc, char *argv[]) {
    printf("csh_getenv(\"%s\")= %s\n", "JB", csh_getvar("JB"));
    printf("csh_setenv(\"%s\", \"%s\")= %d\n", "JB", "SPACEINVENTOR", csh_putvar("JB", "SPACEINVENTOR"));
    printf("csh_getenv(\"%s\")= %s\n", "JB", csh_getvar("JB"));
    char *expanded = csh_expand_vars("$(JB) test $(JB) -v 1 $(JB)");
    printf("csh_expand_vars(\"$(JB) test $(JB) -v 1 $(JB)\")=%s\n", expanded);
    free(expanded);
    
    expanded = csh_expand_vars("$(JB) test $(JB) -v 1 $(JB");
    printf("csh_expand_vars(\"$(JB) test $(JB) -v 1 $(JB\")=%s\n", expanded);
    free(expanded);
    
    printf("csh_delvar(\"%s\")= %d\n", "JB", csh_delvar("JB"));
    printf("csh_getenv(\"%s\")= %s\n", "JB", csh_getvar("JB"));
    expanded = csh_expand_vars("$(JB) test $(JB) -v 1 $(JB");
    printf("csh_expand_vars(\"$(JB) test $(JB) -v 1 $(JB)\")=%s\n", expanded);
    free(expanded);

    printf("csh_setenv(\"%s\", \"%s\")= %d\n", "JB", "SPACEINVENTOR", csh_putvar("JB", "SPACEINVENTOR"));
    printf("csh_getenv(\"%s\")= %s\n", "JB", csh_getvar("JB"));
    printf("csh_clearenv()\n");
    csh_clearenv();
    printf("csh_getenv(\"%s\")= %s\n", "JB", csh_getvar("JB"));
    return 0;
}
#endif