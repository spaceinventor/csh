#include <ctype.h>
#include <string.h>

#include <slash/slash.h>
#include <slash/optparse.h>


typedef struct {
    int major;
    int minor;
    int patch;
} version_t;


bool parse_version(const char *version_str, version_t *version) {
    if (!version_str || !version) return false;

    // Default the version fields to 0
    version->major = version->minor = version->patch = 0;

    // Attempt to parse the version string
    int matched = sscanf(version_str, "%d.%d-%d", 
                         &version->major, 
                         &version->minor, 
                         &version->patch);

    if (matched >= 2) { // Parsed successfully in dash-separated format
        return true;
    } 

    // Try parsing the dot-separated format as fallback
    matched = sscanf(version_str, "%d.%d.%d", 
                     &version->major, 
                     &version->minor, 
                     &version->patch);

    return matched >= 2; // Parsed successfully in dot-separated format
}


static int compare_int(int a, int b) {
    return (a > b) - (a < b); // Returns 1 if a > b, -1 if a < b, 0 if a == b
}

bool compare_version(const version_t *version, const char *constraint) {
    if (!version || !constraint) {
        return false;   
    }

    // Default values for constraint operator and version fields
    char operator[3] = "=="; // Default to equality
    version_t constraint_version = {0, 0, 0};

    // Possible patterns to try
    const char *patterns[] = {
        "%2[<=>!]%d.%d.%d",  // Dot-separated with operator
        "%2[<=>!]%d.%d-%d",  // Dash-separated with operator
        "%d.%d.%d",          // Dot-separated without operator
        "%d.%d-%d",          // Dash-separated without operator
    };

    int max_matched = 0;
    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); ++i) {
        char temp_operator[3] = {0}; // Temporary operator for this pattern
        version_t temp_version = {0, 0, 0};

        int matched = sscanf(constraint, patterns[i],
                             temp_operator,
                             &temp_version.major,
                             &temp_version.minor,
                             &temp_version.patch);

        // Update the best match
        if (matched <= max_matched) {
            continue;  // This is not a better match
        }

        max_matched = matched;

        // Save the best match results
        if (matched >= 1 && strlen(temp_operator) > 0) {
            strncpy(operator, temp_operator, sizeof(operator) - 1);
        }
        constraint_version = temp_version;
    }

    // Ensure at least major and minor versions were parsed
    if (max_matched < 2) {
        return false;
    }

    // Compare the parsed constraint version against the given version
    int major_cmp = compare_int(version->major, constraint_version.major);
    int minor_cmp = compare_int(version->minor, constraint_version.minor);
    int patch_cmp = compare_int(version->patch, constraint_version.patch);

    // Evaluate the operator
    if (strcmp(operator, "==") == 0) {
        return major_cmp == 0 && minor_cmp == 0 && patch_cmp == 0;
    } else if (strcmp(operator, "!=") == 0) {
        return major_cmp != 0 || minor_cmp != 0 || patch_cmp != 0;
    } else if (strcmp(operator, ">=") == 0) {
        return (major_cmp > 0) || (major_cmp == 0 && minor_cmp > 0) ||
               (major_cmp == 0 && minor_cmp == 0 && patch_cmp >= 0);
    } else if (strcmp(operator, "<=") == 0) {
        return (major_cmp < 0) || (major_cmp == 0 && minor_cmp < 0) ||
               (major_cmp == 0 && minor_cmp == 0 && patch_cmp <= 0);
    } else if (strcmp(operator, ">") == 0) {
        return (major_cmp > 0) || (major_cmp == 0 && minor_cmp > 0) ||
               (major_cmp == 0 && minor_cmp == 0 && patch_cmp > 0);
    } else if (strcmp(operator, "<") == 0) {
        return (major_cmp < 0) || (major_cmp == 0 && minor_cmp < 0) ||
               (major_cmp == 0 && minor_cmp == 0 && patch_cmp < 0);
    }

    return false; // Unknown operator
}

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
        /* TODO: We could arguably have Fatal as default */
		printf("missing error-level parameter\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char *error_level = slash->argv[argi];
    *error_level = tolower(*error_level);  // Convert first letter to lower case

    /* Check if user message is present */
	if (++argi < slash->argc) {
        user_message = slash->argv[argi];
	}

    int err_ret = SLASH_EXIT;

    if        (strcasestr("fatal", error_level)) {
        err_ret = SLASH_EXIT;
    } else if (strcasestr("error", error_level)) {
        err_ret = SLASH_EBREAK;
    } else if (strcasestr("warn", error_level)) {
        err_ret = SLASH_EINVAL;
    } else {
        fprintf(stderr, "Unknown error type specfied: '%s'. Choices are: Fatal, Error, Warn\n", error_level);
        optparse_del(parser);
        return SLASH_EINVAL;
    }

    extern const char *version_string;  // VCS tag set by meson.build using version.c.in

    version_t csh_version;
    if (false == parse_version(version_string, &csh_version)) {
        fprintf(stderr, "CSH is tagged with a non semver complaint version '%s'\n", version_string);
        optparse_del(parser);
        /* err_ret may not be the best error code here,
            but its our best guess at what the script author intended. */
        return err_ret;
    }

    if (false == compare_version(&csh_version, version_constraint)) {
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
slash_command_subsub(require, version, csh, slash_require_version_csh_cmd, "<version-constraint> <error-level> [error-message]",\
"Checks whether CSH fulfills the specified version requirements.\n\n "\
"A failed comparison may perform a varity of actions, based on the specified error level.\n "\
"Possible error levels are:\n "\
"- Fatal: Which will exit CSH,\n "\
"- Error: Which will break execution of a script,\n "\
"- Warn: Which simply prints the specified message,\n "\
"(Single letters may be used for error codes, e.g 'F' for Fatal).\n\n "\
"Version constraint supports the typical comparisons: \"==\", \"!=\", \">=\", \"<=\", \">\" and \"<\".\n "\
"For example: >=2.5-20");
