
#define _GNU_SOURCE
#include "require_version.h"
#include <ctype.h>

#include <string.h>

#include <slash/slash.h>
#include <slash/optparse.h>


/* TODO: Should we have either a command or argument for whether to allow dirty version of CSH?
    It would be rather easy to check for the trailing '+' in the version string. */

const struct slash_command slash_cmd_require_versioncsh;
static int slash_require_version_csh_cmd(struct slash *slash) {

    int verbose = false;
    char *user_message = NULL;

    optparse_t * parser = optparse_new_ex(slash_cmd_require_versioncsh.name, slash_cmd_require_versioncsh.args, slash_cmd_require_versioncsh.help);
    optparse_add_set(parser, 'v', "verbose", 1, &verbose, "Verbose comparison");
    optparse_add_help(parser);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    /* Check if version constraint is present */
	if (++argi >= slash->argc) {
		printf("missing version constraint parameter\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char *version_constraint = slash->argv[argi];

    /* Check if version constraint is present */
	if (++argi >= slash->argc) {
        /* TODO: We could arguably have Quit as default */
		printf("missing error-action parameter\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char *error_action = slash->argv[argi];
    *error_action = tolower(*error_action);  // Convert first letter to lower case

    /* Check if user message is present */
	if (++argi < slash->argc) {
        user_message = slash->argv[argi];
	}

    int err_ret = SLASH_EXIT;

    if        (strcasestr("quit", error_action)) {
        err_ret = SLASH_EXIT;
    } else if (strcasestr("error", error_action)) {
        err_ret = SLASH_EBREAK;
    } else if (strcasestr("warn", error_action)) {
        err_ret = SLASH_EINVAL;
    } else {
        fprintf(stderr, "Unknown error type specfied: '%s'. Choices are: Quit, Error, Warn\n", error_action);
        optparse_del(parser);
        return SLASH_EINVAL;
    }

    extern const char *version_string;  // VCS tag set by meson.build using version.c.in

    version_t csh_version;
    if (false == parse_version(version_string, &csh_version, true)) {
        fprintf(stderr, "CSH is tagged with a non semver complaint version '%s'\n", version_string);
        optparse_del(parser);
        /* err_ret may not be the best error code here,
            but its our best guess at what the script author intended. */
        return err_ret;
    }

    if (false == compare_version(&csh_version, version_constraint, true)) {
        /* TODO: Should we print the error message in red, or let the user specify the color code? */
        fprintf(stderr, "\033[31mVersion %s does not satisfy constraint %s", version_string, version_constraint);
        if (user_message) {
            fprintf(stderr, "\n%s", user_message);
        }
        fprintf(stderr, "\033[0m\n");
        optparse_del(parser);
        return err_ret;
    }

    if (verbose) {
        printf("Version %s satisfies constraint %s\n", version_string, version_constraint);
    }

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_subsub(require, version, csh, slash_require_version_csh_cmd, "<version-constraint> <error-action> [error-message]",\
"Checks whether CSH fulfills the specified version requirements.\n\n "\
"A failed comparison may perform a varity of actions, based on the specified error action.\n "\
"Possible error actions are:\n "\
"- Quit: Which will exit CSH,\n "\
"- Error: Which will break execution of a script,\n "\
"- Warn: Which simply prints the specified message,\n "\
"(Single letters may be used for error codes, e.g 'F' for Quit).\n\n "\
"Version constraint supports the typical comparisons: \"==\", \"!=\", \">=\", \"<=\", \">\" and \"<\".\n "\
"For example: >=2.5-20");
