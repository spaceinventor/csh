
#include "walkdir.h"
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>
#include <param/param.h>
#include <param/param_list.h>
#include <dlfcn.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

slash_command_group(addin, "addin");

#define ADDIN_MAX_PATH_SIZE 256
#define ADDIN_SEARCH_MAX 20
#define ADDIN_LIBMAIN_ARGS_MAX 40

/**
 * Load and create list of libraries 
 */

/* Library init signature versions:
    1 = void libmain(void)
    2 = int libmain(void)
*/
__attribute__((used)) const int apm_init_version = 2;  // NOTE: Must be updated when APM init signature(s) change.
typedef int (*libmain_t)(void);
typedef void (*libinfo_t)(void);

typedef struct addin_entry_s addin_entry_t;
struct addin_entry_s {
    void * handle;

    char path[ADDIN_MAX_PATH_SIZE];
    const char * file;

    char args[256];
    libmain_t libmain_f;
    libinfo_t libinfo_f;

    addin_entry_t * next;
};

static addin_entry_t * addin_queue = 0; 
typedef void (*info_t) (void);

static void addin_queue_add(addin_entry_t * e) {

    if (!e) {
        return;
    }

    e->next = addin_queue;
    addin_entry_t * prev = 0;

    while (e->next && (strcmp(e->file, e->next->file) > 0)) {
        prev = e->next;
        e->next = e->next->next;
    }

    if (prev) {
        /* Insert before e->next */
        prev->next = e;
    } else {
        /* Insert as first entry */
        addin_queue = e;
    }

}

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

    strncpy(e->path, path, ADDIN_MAX_PATH_SIZE - 1);

    size_t i = strlen(e->path);
    while ((i > 0) && (e->path[i-1] != '/')) {
        i--;
    }
    e->file = &(e->path[i]);

    /* Get references to addin API functions */
    e->libmain_f = dlsym(handle, "libmain");
    e->libinfo_f = dlsym(handle, "libinfo");

    e->handle = handle;

    addin_queue_add(e);

    return e;

}

void initialize_addin(addin_entry_t * e, struct slash *slash, const char * args) {

    const int * apm_init_version_in_apm_ptr = dlsym(e->handle, "apm_init_version");
    if (apm_init_version_in_apm_ptr == NULL || apm_init_version != *apm_init_version_in_apm_ptr) {
        fprintf(stderr, "APM init function version mismatch, refusing to load %s\n", e->file);
        return;
    }

    int res = 1;
    if (e->libmain_f) {
        res = e->libmain_f();
    }

    if (res) {
        fprintf(stderr, "Failed to add APM \"%s\", self destruction initiated!\n", e->file);
#ifdef SLASH_HAVE_TERMIOS_H
	tcsetattr(slash->fd_read, TCSANOW, &slash->original);
#endif
    exit(1);
    }
}

/**
 * Library search
 */

/* Info on a library */
typedef struct lib_info_s lib_info_t;
struct lib_info_s {
	char path[ADDIN_MAX_PATH_SIZE];
    char time[30];
};

typedef struct lib_search_s lib_search_t;
struct lib_search_s {
    char * path;
    char * file;
    char * search_str;

	unsigned lib_count;
	lib_info_t libs[ADDIN_SEARCH_MAX];
} lib_search;

static char wpath[ADDIN_MAX_PATH_SIZE];

void init_info(lib_info_t * info, const char * path) {

    if (!info) {
        return;
    }

    strncpy(info->path, path, ADDIN_MAX_PATH_SIZE);

    struct stat attrib;
    stat(path, &attrib);
    strftime(info->time, sizeof(info->time) - 1, "%d-%m-%Y %H:%M:%S", localtime(&(attrib.st_ctime)));
}

static bool dir_callback(const char * path_and_file, const char * last_entry, void * custom) {
    /* All directories are searched */
	return true;
}

static void file_callback(const char * path_and_file, const char * last_entry, void * custom) {

    if (!custom) {
        return;
    }

    lib_search_t * search = (lib_search_t*)custom;

    /* Verify info struct is available */
    if (search->lib_count >= ADDIN_SEARCH_MAX) {
        return;
    }

    /* Verify file has search string */
	if (search->search_str && !strstr(last_entry, search->search_str)) {
		return;
	}

    /* Verify file name prefix */
	if (!strstr(last_entry, "libcsh_")) {
		return;
	}

    /* Verify file name suffix */
	if (!strstr(last_entry, ".so")) {
		return;
	}

    /* Initialize info struct */
    lib_info_t * info = &search->libs[search->lib_count];
    init_info(info, path_and_file);

    /* Verify not already loaded */
    for (addin_entry_t * e = addin_queue; e; e = e->next) {
        if (strcmp(e->file, last_entry) == 0) {
            return;
        }
    }

    /* Add info struct to search list */
	search->lib_count++;

}

static char installdir[100] = {};

static const char * get_installdir() {

    if (strlen(installdir) == 0) {
        const char * path = getenv("HOME");
        if (!path) {
            path = getpwuid(getuid())->pw_dir;
        }
        strcpy(installdir, path);
        strcat(installdir, "/.local/lib");
    }
    return installdir;
}

void build_addin_list(char * path, char * search_str, unsigned max_depth) {

    /* Clear search list */
	lib_search.lib_count = 0;
    lib_search.search_str = search_str;

    /* Always search for libraries in ${HOME}/.local/lib */
    strcpy(wpath, get_installdir());
    walkdir(wpath, ADDIN_MAX_PATH_SIZE - 10, max_depth, dir_callback, file_callback, &lib_search);

    if (!path) {
        return;
    }

    /* Split path on ';' and process each path */
    const char * cur_path = path;
    bool done = false;
    for (size_t i = 0; !done; ++i) {
        done = (path[i] == 0);
        if (path[i] == ';') {
            path[i] = 0;
        }
        if ((path[i] == ';') || (path[i] == 0)) {
            /* Terminate and process current path */
            strcpy(wpath, cur_path);
            walkdir(wpath, ADDIN_MAX_PATH_SIZE - 10, max_depth, dir_callback, file_callback, &lib_search);
            /* Prepare next path */
            cur_path = &path[i+1];
        }
    }

}


/**
 * Slash command
 */

static int addin_load_cmd(struct slash *slash) {

	char * path = 0;
	char * search_str = 0;
    int max_depth = 5;
    int yes = 0;
    char * args = 0;

    optparse_t * parser = optparse_new("addin load", "-f <filename> -p <pathname>");
    optparse_add_help(parser);
    optparse_add_string(parser, 'p', "path", "PATHNAME", &path, "Search paths separated by ';'");
    optparse_add_string(parser, 's', "search", "SEARCHSTR", &search_str, "Search string on addin file name");
    optparse_add_int(parser, 'd', "depth", "MAXDEPTH", 10, &max_depth, "Max search depth. Number of levels of entering sub-directories.");
    optparse_add_set(parser, 'y', "yes", 1, &yes, "Accept first found addin.");
    optparse_add_string(parser, 'a', "args", "ARGS", &args, "Arguments passed to libmain.");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    build_addin_list(path, search_str, max_depth);

    if (lib_search.lib_count == 0) {
        printf("\033[31m\n");
        printf("No addins found in %s", get_installdir());
        if (path) {
            printf(";%s", path);
        }
        printf("\n");
        printf("\033[0m\n");
        return SLASH_EUSAGE;
    }

    const char * selected = lib_search.libs[0].path;

    if (!yes) {
        /* Manual selection of file from list */

        for (unsigned i = 0; i < lib_search.lib_count; i++) {
            lib_info_t * info = &lib_search.libs[i];
            printf("  %u: %s %s\n", i, info->time, info->path);
        }

        int index = 0;
        printf("Type number to select: ");
        char * c = slash_readline(slash);
        if (strlen(c) == 0) {
            printf("\033[31m\n");
            printf("Entry (%s) not accepted. Nothing done.\n", c);
            printf("\033[0m\n");
            return SLASH_EUSAGE;
        }
        index = atoi(c);
printf("%d %d\n", __LINE__, index);
        if (index >= lib_search.lib_count) {
            printf("\033[31m\n");
            printf("Value (%d) is out of bounds.\n", index);
            printf("\033[0m\n");
            return SLASH_EUSAGE;
        }

    	selected = lib_search.libs[index].path;
printf("%d %d '%s'\n", __LINE__, index, selected);

        printf("\033[32m\n");
        printf("SELECTED: %s\n", selected);
        printf("\033[0m\n");

        printf("Type 'yes' + enter to continue: ");
        c = slash_readline(slash);
        if (strcmp(c, "yes") != 0) {
            printf("\033[31m\n");
            printf("Entry (%s) not accepted. Nothing done.\n", c);
            printf("\033[0m\n");
            return SLASH_EUSAGE;
        }
    }

printf("selected '%s'\n", selected);

    addin_entry_t * e = load_addin(selected);

    if (!e) {
        printf("\033[31m\n");
        printf("ERROR LOADING: %s\n", e->path);
        printf("\033[0m\n");
        return SLASH_EUSAGE;
    }

    printf("\033[32m\n");
    printf("LOADED: %s\n", e->path);
    printf("\033[0m\n");

    initialize_addin(e, slash, args);

    return SLASH_SUCCESS;

}

slash_command_sub(addin, load, addin_load_cmd, "", "Load an addin");

static int addin_info_cmd(struct slash *slash) {

	char * search_str = 0;

    optparse_t * parser = optparse_new("addin info", "<search>");
    optparse_add_help(parser);
    optparse_add_string(parser, 's', "search", "SEARCHSTR", &search_str, "Search string on addin file name");
 
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    /* Search string is accepted without leading -s */
	if (!search_str && (++argi < slash->argc)) {
		search_str = slash->argv[argi];
	}

    for (addin_entry_t * e = addin_queue; e; e = e->next) {
        if (!search_str || strstr(e->file, search_str)) {
            printf("  %-30s %-80s\n", e->file, e->path);
            if (e->libinfo_f) {
                e->libinfo_f();
            }
        }
        printf("\n");
    }

    return SLASH_SUCCESS;

}

slash_command_sub(addin, info, addin_info_cmd, "", "Information on addins");
