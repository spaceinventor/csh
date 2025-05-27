
#include "walkdir.h"
#include <slash/slash.h>
#include <slash/optparse.h>
#include <param/param.h>
#include <param/param_list.h>
#include <apm/csh_api.h>
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
#include <limits.h>
#include <libgen.h>

#ifdef HAVE_PYTHON
#include "python/python_loader.h"
#endif

slash_command_group(apm, "apm");

/**
 * Load and create list of libraries 
 */

/* Library init signature versions:
    1 = void libmain(void)
    2 = int libmain(void)
*/
static const int apm_init_version = APM_INIT_VERSION;
typedef int (*libmain_t)(void);
typedef void (*libinfo_t)(void);

typedef struct apm_entry_s apm_entry_t;
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

static apm_entry_t * apm_queue = 0; 
typedef void (*info_t) (void);

static void apm_queue_add(apm_entry_t * e) {

    if (!e) {
        return;
    }

    e->next = apm_queue;
    apm_entry_t * prev = 0;

    while (e->next && (strcmp(e->file, e->next->file) > 0)) {
        prev = e->next;
        e->next = e->next->next;
    }

    if (prev) {
        /* Insert before e->next */
        prev->next = e;
    } else {
        /* Insert as first entry */
        apm_queue = e;
    }

}

apm_entry_t * load_apm(const char * path) {

    void * handle = dlopen(path, RTLD_NOW);
    if (!handle)
    {
        printf("Could not load library '%s' %s\n", path, dlerror());
        return NULL;
    }

    apm_entry_t * e = malloc(sizeof(apm_entry_t));

    if (!e) {
        printf("Memory allocation error.\n");
        dlclose(handle);
        return NULL;
    }

    strncpy(e->path, path, WALKDIR_MAX_PATH_SIZE - 1);

    size_t i = strlen(e->path);
    while ((i > 0) && (e->path[i-1] != '/')) {
        i--;
    }
    e->file = &(e->path[i]);

    /* Get references to APM API functions */
    e->libmain_f = dlsym(handle, "libmain");
    e->libinfo_f = dlsym(handle, "libinfo");
    e->apm_init_version = *(int *)dlsym(handle, "apm_init_version");

    e->handle = handle;


    return e;

}

int initialize_apm(apm_entry_t * e) {

    const int * apm_init_version_in_apm_ptr = dlsym(e->handle, "apm_init_version");
    if (apm_init_version_in_apm_ptr == NULL) {
        fprintf(stderr, "APM is missing symbol \"apm_init_version\", refusing to load %s\n", e->file);
        return -1;
    }
    if(apm_init_version != *apm_init_version_in_apm_ptr) {
        switch(apm_init_version) {
            case 9: {
                if (8 == *apm_init_version_in_apm_ptr) {
                    /* CSH with API 9 *can* load APMs 8 or 9 */
                    break;
                } else {
                    fprintf(stderr, "\033[31mError loading %s: Version mismatch: csh (%d) vs apm (%d)\033[0m\n", e->file, apm_init_version, *apm_init_version_in_apm_ptr);
                    return -1;    
                }
            }
            default: {
                fprintf(stderr, "\033[31mError loading %s: Version mismatch: csh (%d) vs apm (%d)\033[0m\n", e->file, apm_init_version, *apm_init_version_in_apm_ptr);
                return -1;
            }
        }
    }

    int res = 1;
    if (e->libmain_f) {
        res = e->libmain_f();
    }

    if (res) {
        fprintf(stderr, "Failed to add APM \"%s\", self destruction initiated!\n", e->file);
        /* main.c atexit() should restore the terminal. */
        exit(1);
    }

    return 0;
}

/**
 * Library search
 */

/* Info on a library */
typedef struct lib_info_s lib_info_t;
struct lib_info_s {
	char path[WALKDIR_MAX_PATH_SIZE];
    char time[30];
};

typedef struct lib_search_s {
    char * path;
    char * search_str;

	unsigned lib_count;
	lib_info_t libs[WALKDIR_MAX_ENTRIES];
} lib_search_t;

void init_info(lib_info_t * info, const char * path) {

    if (!info) {
        return;
    }

    strncpy(info->path, path, WALKDIR_MAX_PATH_SIZE-1);  // -1 to fit NULL byte

    struct stat attrib;
    stat(path, &attrib);
    strftime(info->time, sizeof(info->time) - 1, "%d-%m-%Y %H:%M:%S", localtime(&(attrib.st_ctime)));
}

static bool dir_callback(const char * path_and_file, const char * last_entry, void * custom) {
    (void)path_and_file;
    (void)last_entry;
    (void)custom;
    /* All directories are searched */
	return true;
}

static void file_callback(const char * path_and_file, const char * last_entry, void * custom) {

    if (!custom) {
        return;
    }

    lib_search_t * search = (lib_search_t*)custom;

    /* Verify info struct is available */
    if (search->lib_count >= WALKDIR_MAX_ENTRIES) {
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
    for (apm_entry_t * e = apm_queue; e; e = e->next) {
        if (strcmp(e->file, last_entry) == 0) {
            fprintf(stderr, "\033[33mWarn skipping %s already loaded\033[0m\n", last_entry);
            return;
        }
    }

    /* Verify that we have not already found this APM.
        This can happen when the same path is found multiple times, separated by ';' */
    for (unsigned i = 0; i < search->lib_count; i++) {
        const char * filename = search->libs[i].path;
        const char * const dir = strrchr(search->libs[i].path, '/');

        // search->libs[i].path is not in root, and we only want the filename
        if (dir != NULL) {
            // dir+1 to skip preceding '/' character
            filename = dir+1;
        }

        if (strcmp(filename, last_entry) == 0) {
            return;
        }
    }

    /* Add info struct to search list */
	search->lib_count++;

}

void build_apm_list(lib_search_t* lib_search) {

    /* Clear search list */
	lib_search->lib_count = 0;

    char * path = lib_search->path;
    char wpath[WALKDIR_MAX_PATH_SIZE] = {0};  // Initialization matters here

    /* Split path on ';' and process each path */
    while(path[0] != '\0') {

        char* split = strchr(path, ';');

        if (split != NULL) strncpy(wpath, path, split-path);
        else strcpy(wpath, path);

        walkdir(wpath, WALKDIR_MAX_PATH_SIZE - 10, 1, dir_callback, file_callback, lib_search, NULL);

        if (split == NULL) break;

        path = split+1;
    }

}

int apm_load_search(lib_search_t *lib_search) {

    char path[WALKDIR_MAX_PATH_SIZE] = {0};
    int search_bin_path = 0;

    if (lib_search->path == NULL) {
        char * p = getenv("HOME");
        search_bin_path = 1;
        if (p == NULL) {
            p = getpwuid(getuid())->pw_dir;
            if(p == NULL){
                printf("No home folder found\n");
                return SLASH_EINVAL;  
            }
        }
        strncpy(path, p, WALKDIR_MAX_PATH_SIZE-strnlen(path, WALKDIR_MAX_PATH_SIZE-1)-1);
        strncat(path, "/.local/lib/csh", WALKDIR_MAX_PATH_SIZE-strnlen(path, WALKDIR_MAX_PATH_SIZE-1));
        lib_search->path = path;
    }

    if (search_bin_path) {

        char result[WALKDIR_MAX_PATH_SIZE] = {0};
        int count = readlink("/proc/self/exe", result, WALKDIR_MAX_PATH_SIZE);

        if (count == -1) {
            perror("readlink");
            lib_search->path = NULL;
            return SLASH_EUSAGE;
        }

        const char *dir = dirname(result);

        strncat(path, ";", WALKDIR_MAX_PATH_SIZE-strnlen(path, WALKDIR_MAX_PATH_SIZE));
        strncat(path, dir, WALKDIR_MAX_PATH_SIZE-strnlen(path, WALKDIR_MAX_PATH_SIZE));
    }

    strncat(path, ";", WALKDIR_MAX_PATH_SIZE-strnlen(path, WALKDIR_MAX_PATH_SIZE));
    strncat(path, "/usr/lib/csh", WALKDIR_MAX_PATH_SIZE-strnlen(path, WALKDIR_MAX_PATH_SIZE));

    build_apm_list(lib_search);

    if (lib_search->lib_count == 0) {
        printf("\033[31mNo APMs found in %s\033[0m\n", lib_search->path);
    }

    for (unsigned i = 0; i < lib_search->lib_count; i++) {
        apm_entry_t * e = load_apm(lib_search->libs[i].path);

        if (!e) {
            fprintf(stderr, "\033[31mError loading %s\033[0m\n", lib_search->libs[i].path);
            return SLASH_EUSAGE;
        }

        if(initialize_apm(e) != 0){
            if (e->handle) {
                dlclose(e->handle);
            }
            free(e);
            continue;
        }

        apm_queue_add(e);
        printf("\033[32mLoaded: %s\033[0m\n", e->path);
    }
    lib_search->path = NULL;
    return SLASH_SUCCESS;
}

/**
 * Slash command
 */

static int apm_load_cmd(struct slash *slash) {

    lib_search_t lib_search;
    lib_search.path = NULL;
    lib_search.search_str = NULL;

    optparse_t * parser = optparse_new("apm load", "-f <filename> -p <pathname>");
    optparse_add_help(parser);
    optparse_add_string(parser, 'p', "path", "PATHNAME", &lib_search.path, "Search paths separated by ';'");
    optparse_add_string(parser, 'f', "file", "FILENAME", &lib_search.search_str, "Search string on APM file name");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    int res = apm_load_search(&lib_search);
    optparse_del(parser);
#ifdef HAVE_PYTHON
    py_init_interpreter();
    res = py_apm_load_cmd(slash);
#endif
    return res;
}

slash_command_sub(apm, load, apm_load_cmd, "", "Load an APM");

static int apm_info_cmd(struct slash *slash) {

	char * search_str = 0;

    optparse_t * parser = optparse_new("apm info", "<search>");
    optparse_add_help(parser);
    optparse_add_string(parser, 's', "search", "SEARCHSTR", &search_str, "Search string on APM file name");
 
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    /* Search string is accepted without leading -s */
	if (!search_str && (++argi < slash->argc)) {
		search_str = slash->argv[argi];
	}

    printf("\033[1mCSH host API: \033[32m%d\033[0m\n\n", apm_init_version);

    for (apm_entry_t * e = apm_queue; e; e = e->next) {
        if (!search_str || strstr(e->file, search_str)) {
            printf("  \033[32m%-30s\033[0m %-80s\tCSH API: %d\n", e->file, e->path, e->apm_init_version);
            if (e->libinfo_f) {
                e->libinfo_f();
            }
            printf("\n");
        }
    }

    return SLASH_SUCCESS;

}
slash_command_sub(apm, info, apm_info_cmd, "", "Information on APMs");

static int doc_cmd(struct slash *slash) {

    int res = system("xdg-open /usr/share/si-csh/CSH_MAN.pdf >/dev/null 2>/dev/null");
    if(res) {
        printf("Could not open CSH PDF manual\n");
    } else {
        printf("CSH manual is now open in PDF viewer, if available\n");
    }

    return SLASH_SUCCESS;
}
slash_command(manual, doc_cmd, "", "Show CSH documentation as PDF");
