
#include "walkdir.h"
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>
#include <param/param.h>
#include <param/param_list.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

slash_command_group(addin, "addin");

#define ADDIN_MAX_PATH_SIZE 256
#define ADDIN_SEARCH_MAX 10

struct lib_info_t {
	unsigned count;
	char entries[ADDIN_SEARCH_MAX][ADDIN_MAX_PATH_SIZE];
} lib_info;

static char wpath[ADDIN_MAX_PATH_SIZE];

typedef void (*libmain_t)(int argc, char ** argv);
typedef void (*libinfo_t)(void);
typedef void (*get_slash_ptrs_t)(struct slash_command ** start, struct slash_command ** stop);
typedef void (*get_param_ptrs_t)(param_t ** start, param_t ** stop);
typedef void (*get_vmem_ptrs_t)(vmem_t ** start, vmem_t ** stop);

typedef struct addin_entry_s addin_entry_t;
struct addin_entry_s {
    void * handle;

    char path[ADDIN_MAX_PATH_SIZE];
    const char * file_name;

    libmain_t libmain_f;
    libinfo_t libinfo_f;
	get_slash_ptrs_t get_slash_ptrs_f;
	get_param_ptrs_t get_param_ptrs_f;
	get_vmem_ptrs_t get_vmem_ptrs_f;

    addin_entry_t * next;
};

static addin_entry_t * addin_queue = 0; 
typedef void (*info_t) (void);

addin_entry_t * load_addin(const char * path) {

    void * handle = dlopen(path, RTLD_NOW);
    if (!handle)
    {
        printf("Could no load library '%s' %s\n", path, dlerror());
        return 0;
    }

    addin_entry_t * e = malloc(sizeof(addin_entry_t));

    if (!e) {
        printf("Memmory allocation error.\n");
        dlclose(handle);
        return 0;
    }

    /* Add to queue */
    e->next = addin_queue;
    addin_queue = e;

    strncpy(e->path, path, ADDIN_MAX_PATH_SIZE - 1);

    size_t i = strlen(path);
    while ((i > 0) && (path[i-1] != '/')) {
        i--;
    }
    e->file_name = &(path[i]);

    /* Get references to addin API functions */
    e->libmain_f = dlsym(handle, "libmain");
    e->libinfo_f = dlsym(handle, "libinfo");
    e->get_slash_ptrs_f = dlsym(handle, "get_slash_pointers");
    e->get_param_ptrs_f = dlsym(handle, "get_param_pointers");
    e->get_vmem_ptrs_f = dlsym(handle, "get_vmem_pointers");

    e->handle = handle;

    return e;

}

bool is_valid_library(const char * path, const char * file_name, struct lib_info_t * libinf)
{
	int len = strlen(path);
	if ((len <= 3) || (strcmp(&(path[len-3]), ".so") != 0)) {
		return false;
	}

    /* Check if already loaded */
    addin_entry_t * e = addin_queue;
    while (e && (strcmp(e->file_name, file_name))) {
        e = e->next;
    }

    if (e) {
        /* Already loaded */
        return false;
    }

	return true;
}

static bool dir_callback(const char * path, const char * last_entry, void * custom) 
{
    /* All directories are searched */
	return true;
}

static void file_callback(const char * path, const char * last_entry, void * custom)
{
    struct lib_info_t * libinf = (struct lib_info_t *)custom; 
	if (libinf && is_valid_library(path, last_entry, libinf)) {
		if (libinf->count < ADDIN_SEARCH_MAX) {
			strncpy(libinf->entries[libinf->count], path, ADDIN_MAX_PATH_SIZE);
			libinf->count++;

        }
		else {
			printf("Search terminated at %u libraries. There may be more.\n", ADDIN_SEARCH_MAX);
		}
	}
}

static int addin_load_cmd(struct slash *slash) {

	char * path = NULL;

    optparse_t * parser = optparse_new("addin load", "<filename>");
    optparse_add_help(parser);
    optparse_add_string(parser, 'f', "file", "FILENAME", &path, "File to load (defaults to select from list)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (!path) {
        /* Manual selection of file from list */

		printf("  Searching for addins\n");
		strcpy(wpath, ".");
		lib_info.count = 0;
		walkdir(wpath, ADDIN_MAX_PATH_SIZE - 10, 10, dir_callback, file_callback, &lib_info);

		if (lib_info.count == 0) {
			printf("\033[31m\n");
			printf("  Found no addin.\n");
			printf("\033[0m\n");
			return SLASH_EINVAL;
		}

        for (unsigned i = 0; i < lib_info.count; i++) {
            printf("  %u: %s\n", i, lib_info.entries[i]);
        }

        int index = 0;
        printf("Type number to select file: ");
        char * c = slash_readline(slash);
        if (strlen(c) == 0) {
            printf("Abort\n");
            return SLASH_EUSAGE;
        }
        index = atoi(c);

        if (index >= ADDIN_SEARCH_MAX) {
            printf("Value (%d) is out of bounds.\n", index);
            return SLASH_EUSAGE;
        }

    	path = lib_info.entries[index];

        printf("\033[31m\n");
        printf("SELECTED: %s\n", path);
        printf("\033[0m\n");

        printf("Type 'yes' + enter to continue: ");
        c = slash_readline(slash);
        if (strcmp(c, "yes") != 0) {
            printf("Abort\n");
            return SLASH_EUSAGE;
        }
    }

    addin_entry_t * e = load_addin(path);

    if (!e) {
        printf("Error loading library '%s' '%s'\n", e->path, e->file_name);
        return SLASH_EUSAGE;
    }

    printf("\033[31m\n");
    printf("INITIALIZING: %s\n", e->file_name);
    printf("\033[0m\n");

    if (e->libmain_f) {
        e->libmain_f(slash->argc, slash->argv);
    }

    if (e->get_slash_ptrs_f) {
        struct slash_command * start;
        struct slash_command * stop;
        e->get_slash_ptrs_f(&start, &stop);
        slash_command_list_add(slash, start, stop);
    }

    if (e->get_param_ptrs_f) {
        param_t * start;
        param_t * stop;
        e->get_param_ptrs_f(&start, &stop);
        param_list_add_section(param_list_head(), start, stop);
    }

    if (e->get_vmem_ptrs_f) {
        vmem_t * start;
        vmem_t * stop;
        e->get_vmem_ptrs_f(&start, &stop);
        vmem_list_add_section(vmem_list_head(), start, stop);
    }

    return SLASH_SUCCESS;

}

slash_command_sub(addin, load, addin_load_cmd, "", "Load an addin");

static int addin_info_cmd(struct slash *slash) {

    printf("Addin information\n");

    for (addin_entry_t * e = addin_queue; e; e = e->next) {
        printf("  %-30s %-80s\n", e->file_name, e->path);
        if (e->libinfo_f) {
            e->libinfo_f();
        }
        printf("\n");
    }

    return SLASH_SUCCESS;

}

slash_command_sub(addin, info, addin_info_cmd, "", "Information on addins");
