#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <apm/environment.h>

#define ENV_FILE "__FILE__"
#define ENV_FILE_DIR "__FILE_DIR__"

typedef struct {
    char *old_file;
    char *old_file_dir;
} run_context_t;

/* Nested "run <file>.csh" commands look outwards,
    to see if there is an environemnt variable that we will restore to.
    If not, we should clean up. */
void slash_on_run_pre_hook(const char * const filename, void ** ctx_for_post) {  /* Set up environemnt variables for "run" command. */

        /* Important to calloc() (instead of malloc()),
            as we would otherwise end up calling csh_delvar() on uninitialized fields for the outermost "run <file>.csh file." */
        run_context_t * const post_ctx = calloc(1, sizeof(run_context_t));
        if (post_ctx != NULL) {  /* If we don't have enough memory for the context pointers, we can make do by leaving them in the environment. */
            const char * old_file = csh_getvar(ENV_FILE);
            if (old_file != NULL) {
                post_ctx->old_file = strdup(old_file);  // Make a copy, because we are going to override the environemnt.
            }
            const char * old_file_dir = csh_getvar(ENV_FILE_DIR);
            if (old_file_dir != NULL) {
                post_ctx->old_file_dir = strdup(old_file_dir);  // Make a copy, because we are going to override the environemnt.
            }
            *ctx_for_post = post_ctx;
        }

        {  /* Setting environment variables for the "run" script to use. */

            char pathbuf[PATH_MAX] = {0};
            /* Set environment variable for init script file/directory, before running it. */
            if (NULL == realpath(filename, pathbuf)) {
                pathbuf[0] = '\0';
            }
            /* NOTE: pathbuf can be a directory here, which is not the intention.
                It's a pretty inconsequential edge case though, so there's not much need to fix it. */
            csh_putvar(ENV_FILE, pathbuf);

            /* Init file found, and space for trailing '/' */
            if (strnlen(pathbuf, PATH_MAX) > 0) {

                /* Find init script dir by terminating at the last '/'.
                    Removing the last '/' will keep init scripts more readable. */
                /* NOTE: This will not handle multiple invalid init files involving '/', such as:
                    -i init.csh/
                    -i /home/user  # <-- User being a directory
                */
                char *last_slash = strrchr(pathbuf, '/');
                if (last_slash < pathbuf+PATH_MAX-1) {
                    *(last_slash) = '\0';
                }
            }

            csh_putvar(ENV_FILE_DIR, pathbuf);
        }
}

/* Restore environment right as we exit the inner/nested "run <file>.csh",
        so that references to the environment variables in the outer file refer to the correct paths again. */
void slash_on_run_post_hook(const char * const filename, void * ctx) {

    /* We require context from pre-hook to restore.
        Maybe we are out of memory? In any case there's nothing we can do. */
    if (ctx == NULL) {
        return;
    }

    run_context_t * restore_context = (run_context_t*)ctx;

    if (restore_context->old_file == NULL) {
        csh_delvar(ENV_FILE);
    } else {
        csh_putvar(ENV_FILE, restore_context->old_file);
        free(restore_context->old_file);  // Free our copy, now that the environment has its own.
    }

    if (restore_context->old_file_dir == NULL) {
        csh_delvar(ENV_FILE_DIR);
    } else {
        csh_putvar(ENV_FILE_DIR, restore_context->old_file_dir);
        free(restore_context->old_file_dir);  // Free our copy, now that the environment has its own.
    }

    free(ctx);
}
