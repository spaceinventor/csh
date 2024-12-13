#include <ctype.h>
#include <string.h>

#include <slash/slash.h>
#include <slash/optparse.h>


typedef struct {
    int major;
    int minor;
    int patch;
} version_t;


// TODO: We should probably reuse the parsing logic from compare_version(), but then we need a separate function for parse operator.
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

bool compare_version(const version_t *version, const char *constraint_dirty) {
    if (!version || !constraint_dirty) {
        return false;
    }

    /* Sanitize provided constraint */
    #define CONSTRAINT_MAXLEN sizeof(">=v9999.9999.9999")
    #define DEFAULT_OPERATOR "=="
    #define OPERATOR_MAXLEN sizeof(DEFAULT_OPERATOR)
    char operator[OPERATOR_MAXLEN] = DEFAULT_OPERATOR;
    char constraint[CONSTRAINT_MAXLEN] = {0};

    version_t constraint_version = {0, 0, 0};
    #define NUM_VERSION_SEPERATORS 3
    enum  __attribute__((packed)) {
        MAJOR = 0,
        MINOR = 1,
        PATCH = 2,
    };
    bool wildcard_arr[NUM_VERSION_SEPERATORS] = {false};

    for (size_t clean_idx = 0, dirty_idx = 0, dot_count = 0;
        (clean_idx < CONSTRAINT_MAXLEN) && (constraint_dirty[dirty_idx] != '\0') && (dot_count < NUM_VERSION_SEPERATORS);
        clean_idx++, dirty_idx++) {

        /* Allow first 2 letters in dirty constraint as operator */
        if (dirty_idx < OPERATOR_MAXLEN && (constraint_dirty[dirty_idx] == '<' || constraint_dirty[dirty_idx] == '>' || constraint_dirty[dirty_idx] == '=' || constraint_dirty[dirty_idx] == '!')) {
            clean_idx--;  /* Don't skip clean index */
            operator[dirty_idx] = constraint_dirty[dirty_idx];

            if (constraint_dirty[dirty_idx] == '=') {
                continue;
            }

            /* zero the remaining parts of the operator, so '>' doesn't become '>=' because of our default. */
            for (size_t i = dirty_idx+1; i < OPERATOR_MAXLEN; i++) {
                operator[i] = '\0';
            }
            
            continue;
        }

        switch (constraint_dirty[dirty_idx]) {
            case 'v':
            case 'V':
                /* The letter 'v' has no place in our semantic versions */
                clean_idx--;  /* Don't skip clean index */
                continue;

            case '-':  /* Replace '-' with '.' */
            case '.':
                constraint[clean_idx] = '.';
                dot_count++;  // Also track '-' as a '.'
                continue;

            case '*':
                wildcard_arr[dot_count] = true;  // Set the corresponding wildcard flag
                constraint[clean_idx] = '0';  // Include '0' as a placeholder in the sanitized constraint
                /* NOTE: We may get multiple consecutive zeros here, but that shouldn't confuse sscanf(). */
                continue;
            
            default:
                constraint[clean_idx] = constraint_dirty[dirty_idx];
                break;
        }
    }

    /* NOTE: Repeating these operators in the 'for' loop below seems a bit smelly.
        But we're unlikely to add more operators in the future, so it's probably fine. */
    if (!(strcmp(operator, "==") == 0) ||
        (strcmp(operator, "!=") == 0) ||
        (strcmp(operator, ">=") == 0) ||
        (strcmp(operator, "<=") == 0) ||
        (strcmp(operator, ">")  == 0) ||
        (strcmp(operator, "<")) == 0) {
            fprintf(stderr, "\033[31mUnknown version constraint operator \"%s\"\033[0m\n", operator);
            return false;
        }

    /* Parse the sanitized constraint */
    sscanf(constraint, "%d.%d.%d", &constraint_version.major, &constraint_version.minor, &constraint_version.patch);

#if 0
    /* Debug */
    printf("Actual version: %d.%d.%d\n", version->major, version->minor, version->patch);
    printf("Constraint: %s\n", constraint);
    printf("Parsed constraint: %d.%d.%d\n", constraint_version.major, constraint_version.minor, constraint_version.patch);
    printf("Parsed operator %s\n", operator);
    printf("Wildcards %d.%d.%d\n", wildcard_arr[MAJOR], wildcard_arr[MINOR], wildcard_arr[PATCH]);
#endif

    /* Array-based comparison for major, minor, and patch */
    int version_parts[NUM_VERSION_SEPERATORS] = {version->major, version->minor, version->patch};
    int constraint_parts[NUM_VERSION_SEPERATORS] = {constraint_version.major, constraint_version.minor, constraint_version.patch};

    // Compare each version component
    for (int i = 0; i < NUM_VERSION_SEPERATORS; ++i) {

        if (wildcard_arr[i]) {  /* No comparison for wildcards. */
            /* We don't care to support odd constraints such as 2.*.27.
                So we can skip parsing patch if we find a wildcard for minor, etc. */
            break;
        }

        int cmp = (version_parts[i] - constraint_parts[i]);
        if ((strcmp(operator, "==") == 0 && cmp != 0) ||
            (strcmp(operator, "!=") == 0 && cmp == 0) ||
            (strcmp(operator, ">=") == 0 && cmp < 0) ||
            (strcmp(operator, "<=") == 0 && cmp > 0) ||
            (strcmp(operator, ">") == 0 && cmp <= 0) ||
            (strcmp(operator, "<") == 0 && cmp >= 0)) {
            return false;  // Mismatch found
        }
    }

    return true; // Unknown operator
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
