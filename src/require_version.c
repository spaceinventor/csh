
#include "require_version.h"

#include <errno.h>
#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

/* We have kept ourselves quite flexible/generic in regards to adding more parts to the version, i.e: 3.4.3.4,
    not that we expect this to happen. */
#define NUM_VERSION_PARTS 3


bool parse_version(const char *version_str, version_t *version_out, bool allow_suffix) {
    if (!version_str || !version_out) return false;

    // Default the version fields to 0
    version_out->major = version_out->minor = version_out->patch = 0;

    int *semver_out[NUM_VERSION_PARTS] = {&version_out->major, &version_out->minor, &version_out->patch};

    const char *number_start = NULL;

    size_t dot_count = 0;
    for (size_t i = 0; version_str[i] != '\0'; i++) {
        assert(semver_out[dot_count] != NULL);  // This may fail if we change NUM_VERSION_PARTS
        
        switch (version_str[i]) {

            case 'v':
            case 'V': {
                number_start = NULL;
                if (i > 0) {
                    /* Only allow a single prefixed 'v' */
                    return false;
                }
                continue;
            }

            case '-':  /* Parse '-' as '.' */
            case '.': {
                if (number_start == NULL) {
                    /* We have multiple dots in a row, or a dot before a number, either way this is an invalid format. */
                    return false;
                }
                dot_count++;  // Go to next out index: major -> minor
                if (dot_count >= NUM_VERSION_PARTS) {
                    /* Dot overload, too many dots! We can't do 3.4.5.3.56.5.6 */
                    return allow_suffix;
                }
                number_start = NULL;
                continue;
            }

            default: {  /* Valid digits but also other invalid characters. */
                if (!isdigit(version_str[i])) {
                    return false;  // Invalid character.
                }
                if (number_start == NULL) {
                    number_start = &version_str[i];
                    const long temp_long_ver = strtol(number_start, NULL, 10);
                    /* We're supposed to also check if 'errno == ERANGE' here,
                        but for some reason parsing "3" in "v3" gives ERANGE.  */
                    if (temp_long_ver > INT32_MAX) {  
                        return false;  /* Conversion overflow. */
                    }
                    *semver_out[dot_count] = temp_long_ver;
                }
                continue;
            }
        }
    }

    if (number_start == NULL) {
        /* There was likely a trailing dot, anyway wrong format */
        return false;
    }

    /* Parse this last remaining version part. */
    *semver_out[dot_count] = atoi(number_start);

    /* We have finished and successfully parsed the version, including: major, minor and or patch */
    return true;
    
}

bool compare_version(const version_t *version, const char *constraint_dirty, bool verbose) {
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
    enum  __attribute__((packed)) {
        MAJOR = 0,
        MINOR = 1,
        PATCH = 2,
    };
    bool wildcard_arr[NUM_VERSION_PARTS] = {false};

    for (size_t clean_idx = 0, dirty_idx = 0, dot_count = 0;
        (clean_idx < CONSTRAINT_MAXLEN) && (constraint_dirty[dirty_idx] != '\0') && (dot_count < NUM_VERSION_PARTS);
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
                if (dot_count >= NUM_VERSION_PARTS) {
                    /* Dot overload, too many dots! We can't do 3.4.5.3.56.5.6 */
                    return false;
                }
                continue;

            case '*':
                wildcard_arr[dot_count] = true;  // Set the corresponding wildcard flag
                constraint[clean_idx] = '0';  // Include '0' as a placeholder in the sanitized constraint
                /* NOTE: We may get multiple consecutive zeros here, but that shouldn't confuse sscanf(). */
                continue;
            
            default:
                if (!isdigit(constraint_dirty[dirty_idx])) {
                    return false;  // Invalid character.
                }
                constraint[clean_idx] = constraint_dirty[dirty_idx];
                break;
        }
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

    /* Create a single summed integer for the version and constraint,
        which account for their hierarchical nature. */
    long version_scaled = 0;
    long constraint_scaled = 0;
    {
        /* Array-based comparison for major, minor, and patch */
        const int version_parts[NUM_VERSION_PARTS] = {version->major, version->minor, version->patch};
        const int constraint_parts[NUM_VERSION_PARTS] = {constraint_version.major, constraint_version.minor, constraint_version.patch};

        /* Apply scaling to convert versions into comparable integers */
        #define MAJOR_SCALE (1000000)
        #define MINOR_SCALE (1000)
        #define PATCH_SCALE (1)
        const long scaling_factors[NUM_VERSION_PARTS] = {MAJOR_SCALE, MINOR_SCALE, PATCH_SCALE};

        for (size_t i = 0; i < NUM_VERSION_PARTS; i++) {
            if (wildcard_arr[i]) {
                break;  // Don't compare from wildcard and down, by essentially leaving them zeroed.
            }

            version_scaled += version_parts[i] * scaling_factors[i];
            constraint_scaled += constraint_parts[i] * scaling_factors[i];
        }
    }

    /* Evaluate the operator */
    if (strcmp(operator, "==") == 0) {
        return version_scaled == constraint_scaled;
    } else if (strcmp(operator, "!=") == 0) {
        return version_scaled != constraint_scaled;
    } else if (strcmp(operator, ">=") == 0) {
        return version_scaled >= constraint_scaled;
    } else if (strcmp(operator, "<=") == 0) {
        return version_scaled <= constraint_scaled;
    } else if (strcmp(operator, ">") == 0) {
        return version_scaled > constraint_scaled;
    } else if (strcmp(operator, "<") == 0) {
        return version_scaled < constraint_scaled;
    }

    if (verbose) {  // Mostly for guarded by verbose flag for the sake of unit tests.
        fprintf(stderr, "\033[31mUnknown version constraint operator \"%s\"\033[0m\n", operator);
    }
    return false;  // Unknown operator
}
