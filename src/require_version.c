
#include "require_version.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>


// TODO: We should probably reuse the parsing logic from compare_version(), but then we need a separate function for parse operator.
bool parse_version(const char *version_str, version_t *version_out) {
    if (!version_str || !version_out) return false;

    // Default the version fields to 0
    version_out->major = version_out->minor = version_out->patch = 0;

    // Attempt to parse the version string
    int matched = sscanf(version_str, "%d.%d-%d", 
                         &version_out->major, 
                         &version_out->minor, 
                         &version_out->patch);

    if (matched >= 2) { // Parsed successfully in dash-separated format
        return true;
    } 

    // Try parsing the dot-separated format as fallback
    matched = sscanf(version_str, "%d.%d.%d", 
                     &version_out->major, 
                     &version_out->minor, 
                     &version_out->patch);

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
