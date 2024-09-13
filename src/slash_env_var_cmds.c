#include <slash/slash.h>
#include <slash/optparse.h>
#include <stdlib.h>
#include <string.h>
#include "environment.h"
#include "slash_env_var_completion.h"

/**
 * Adds the following commands to CSH:
 * - var_set
 * - var_get
 * - var_unset
 * - var_show
 * - var_clear
 * - var_expand
 * 
 * Examples:
 * 
 * var_set WATCH_TIME 2000
 * var_set CMD "list"
 * var_show
 *
 * var_expand "watch -n $(WATCH_TIME) $(CMD)"
 * - prints the expanded version of its argument
 * 
 * var_expand -e "watch -n $(WATCH_TIME) $(CMD)"
 * -e means "csh-execute the expanded argument"
 * 
 * var_unset CMD
 * var_show
 * 
 * var_clear
 * var_show
 * 
 * var_set MY_PASSWORD 123456 # Don't you ever do that!
 * var_expand -q -e "command -p $(PASSWORD)"
 * -q means do NOT print the expanded version of its argument, but just csh-execute it.
 * This is useful if you do not want the "PASSWORD" variable to appear in logs, for instance. The log will still
 * contain this 'var_expand -q -e "command -p $(PASSWORD)"', which is still useful enough for tracing/debugging without
 * leaking sensitive info.
 */

typedef struct {
    const char *slash_buffer_start;
    const char *previous_match;
    char *to_match;
    int matches;
    int prev_match_length;
    struct slash * slash;
} env_var_completer_t;

static void env_var_completer_csh_foreach_var_cb(const char *name, void *ctx) {
    env_var_completer_t *my_ctx = (env_var_completer_t *)ctx;
    if(strncmp(my_ctx->to_match, name, slash_min(strlen(name), strlen(my_ctx->to_match))) == 0) {
        int cur_l = strlen(name);
        my_ctx->matches++;
        if(NULL != my_ctx->previous_match) {
            if(cur_l <= my_ctx->prev_match_length && strlen(my_ctx->to_match) < my_ctx->prev_match_length) {
                // my_ctx->previous_match = name;
                my_ctx->prev_match_length = cur_l;
            } else {
                my_ctx->matches--;
            }
        } else {
            my_ctx->previous_match = name;
            my_ctx->prev_match_length = cur_l;
        }
        switch(my_ctx->matches) {
            case 1:
                break;
            case 2:
                slash_printf(my_ctx->slash, "\n%s\n", my_ctx->previous_match);
            default:
                slash_printf(my_ctx->slash, "%s\n", name);
                break;
        }
    }
}

static void env_var_completer(struct slash * slash, char * token) {
    int length = strlen(token);
    char *var_start;
    char *var_end;
    bool has_completed = false;
    for (int i = length - 1; i >= 0; i--) {
        if(i >= 0) {
            if(token[i] == ' ') {
                var_start = var_end = &(token[i+1]);
                while(*var_end != '\0' && *var_end != ' ') {
                    var_end++;
                }
                env_var_completer_t ctx = { 
                    .slash_buffer_start = var_start,
                    .to_match = strndup(var_start, var_end - var_start + 1),
                    .matches = 0,
                    .prev_match_length = 0,
                    .slash = slash
                    };
                csh_foreach_var(env_var_completer_csh_foreach_var_cb, &ctx);
                free(ctx.to_match);
                    switch(ctx.matches) {
                        case 0:
                            break;
                        case 1: {
                            strcpy(slash->buffer + (ctx.slash_buffer_start - slash->buffer), ctx.previous_match);
                        }
                        break;
                        default: {
                            strncpy(slash->buffer + (ctx.slash_buffer_start - slash->buffer), ctx.previous_match, ctx.prev_match_length);
                        }
                        break;
                    }
                if(ctx.matches > 0) {
                    slash->length = strlen(slash->buffer);
                    slash->cursor = slash->length;
                }
                has_completed = true;
                break;
            }
        }
    }
    if(false == has_completed) {
        var_start = var_end = token;
        while(*var_end != '\0' && *var_end != ' ') {
            var_end++;
        }
        env_var_completer_t ctx = { 
            .slash_buffer_start = var_start,
            .to_match = strndup(var_start, var_end - var_start + 1),
            .matches = 0,
            .prev_match_length = 0,
            .slash = slash
            };
        csh_foreach_var(env_var_completer_csh_foreach_var_cb, &ctx);
        free(ctx.to_match);
        switch(ctx.matches) {
            case 0:
                break;
            case 1: {
                strcpy(slash->buffer + (ctx.slash_buffer_start - slash->buffer), ctx.previous_match);
            }
            break;
            default: {
                strncpy(slash->buffer + (ctx.slash_buffer_start - slash->buffer), ctx.previous_match, ctx.prev_match_length);
                slash->buffer[(ctx.slash_buffer_start - slash->buffer) + ctx.prev_match_length] = '\0';
            }
            break;
        }
        if(ctx.matches > 0) {
            slash->length = strlen(slash->buffer);
            slash->cursor = slash->length;
        }
    }
}


slash_command_group(env, "CSH environment variables");

const struct slash_command slash_cmd_var_set;
static int slash_var_set(struct slash *slash)
{
    optparse_t * parser = optparse_new_ex("var_set", "NAME VALUE", slash_cmd_var_set.help);
    optparse_add_help(parser);
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi == -1) {
        optparse_del(parser);
        return SLASH_EINVAL;
    }
    if ((slash->argc - argi) != 3) {
        printf("Must give NAME and VALUE parameters\n");
        optparse_del(parser);
	    return SLASH_EINVAL;
    }
    csh_putvar(slash->argv[1], slash->argv[2]);
    printf("Variable %s defined, value=%s\n", slash->argv[1], csh_getvar(slash->argv[1]));
    optparse_del(parser);    
	return SLASH_SUCCESS;
}
slash_command_completer(var_set, slash_var_set, env_var_completer, "NAME VALUE", "Create or update an environment variable in CSH");


const struct slash_command slash_cmd_var_unset;
static int slash_var_unset(struct slash *slash)
{
    optparse_t * parser = optparse_new_ex("var_unset", "NAME", slash_cmd_var_unset.help);
    optparse_add_help(parser);
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi == -1) {
        optparse_del(parser);
        return SLASH_EINVAL;
    }
    if ((slash->argc - argi) != 2) {
        printf("Must give NAME, and only NAME parameter\n");
        optparse_del(parser);
	    return SLASH_EINVAL;
    }
    csh_delvar(slash->argv[1]);
    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command(var_unset, slash_var_unset, "NAME", "Remove an environment variable in CSH");

const struct slash_command slash_cmd_var_clean;
static int slash_var_clear(struct slash *slash)
{
    optparse_t * parser = optparse_new_ex("var_clear", NULL, slash_cmd_var_clean.help);
    optparse_add_help(parser);
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi == -1) {
        optparse_del(parser);
        return SLASH_EINVAL;
    }
    if ((slash->argc - argi) > 1) {
        printf("var_clear takes no parameters\n");
        optparse_del(parser);
	    return SLASH_EINVAL;
    }
    csh_clearenv();
    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command(var_clear, slash_var_clear, NULL, "Clear all environment variables in CSH");

void env_var_ref_completer(struct slash * slash, char * token) {
    int length = strlen(token);
    char *var_start;
    char *var_end;
    bool closing_bracket = false;
    for (int i = length - 1; i >= 0; i--) {
        if(token[i] == ')') {
            closing_bracket = true;
            continue;
        }
        if(i >= 0) {
            if(token[i] == '$' && token[i+1] == '(') {
                if(closing_bracket == false) {
                    var_start = var_end = &(token[i+2]);
                    while(*var_end != '\0' && *var_end != ' ') {
                        var_end++;
                    }
                    env_var_completer_t ctx = { 
                        .slash_buffer_start = var_start,
                        .to_match = strndup(var_start, var_end - var_start + 1),
                        .matches = 0,
                        .prev_match_length = 0,
                        .slash = slash
                     };
                    csh_foreach_var(env_var_completer_csh_foreach_var_cb, &ctx);
                    free(ctx.to_match);
                    switch(ctx.matches) {
                        case 0:
                            break;
                        case 1: {
                            strcpy(slash->buffer + (ctx.slash_buffer_start - slash->buffer), ctx.previous_match);
                            strcat(slash->buffer, ")");
                        }
                        break;
                        default: {
                            strncpy(slash->buffer + (ctx.slash_buffer_start - slash->buffer), ctx.previous_match, ctx.prev_match_length);
                        }
                        break;
                    }
                    if(ctx.matches > 0) {
                        slash->length = strlen(slash->buffer);
                        slash->cursor = slash->length;
                    }
                    break;
                } else {
                    /* We found an entire variable reference from the end of the string, break out of completion */
                    break;
                }
            }
        }
    }
}

const struct slash_command slash_cmd_var_get;
static int slash_var_get(struct slash *slash)
{
    optparse_t * parser = optparse_new_ex("var_get", "NAME", slash_cmd_var_get.help);
    optparse_add_help(parser);
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi == -1) {
        optparse_del(parser);
        return SLASH_EINVAL;
    }
    if ((slash->argc - argi) != 2) {
        printf("Must give NAME, and only NAME parameter\n");
        optparse_del(parser);
	    return SLASH_EINVAL;
    }
    char *value = csh_getvar(slash->argv[1]);
    if(value) {
        printf("%s\n", value);
    }
    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_completer(var_get, slash_var_get, env_var_completer, "NAME", "Show the value of the given 'NAME' environment variable in CSH, shows nothing if variable is not defined");

static void print_var(const char *name, void *ctx) {
    printf("%s=%s\n", name, csh_getvar(name));
}

const struct slash_command slash_cmd_var_show;
static int slash_var_show(struct slash *slash)
{
    optparse_t * parser = optparse_new_ex("var_show", NULL, slash_cmd_var_show.help);
    optparse_add_help(parser);
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi == -1) {
        optparse_del(parser);
        return SLASH_EINVAL;
    }
    if ((slash->argc - argi) > 1) {
        printf("var_show takes no parameters\n");
        optparse_del(parser);
	    return SLASH_EINVAL;
    }
    csh_foreach_var(print_var, NULL);
    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command(var_show, slash_var_show, NULL, "Print the defined variables and their values");

const struct slash_command slash_cmd_var_expand;
static int slash_var_expand(struct slash *slash)
{
    int result = SLASH_SUCCESS;
    int execute = 0;
    int quiet = 0;
    optparse_t * parser = optparse_new_ex("var_expand", "[-e] [-q] INPUT", slash_cmd_var_expand.help);
    optparse_add_help(parser);
    optparse_add_set(parser, 'e', "execute", true, &execute, "Attempt to run the result of the expanded string");
    optparse_add_set(parser, 'q', "quiet", true, &quiet, "Do not print the expanded string before executing it (useful if line contains sensitive info that you do not want logged)");
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi == -1) {
        optparse_del(parser);
        return SLASH_EINVAL;
    }
    if ((slash->argc - argi) != 2) {
        printf("Must give an INPUT parameter\n");
        optparse_del(parser);
	    return SLASH_EINVAL;
    }
    char *expansion;
    if(!execute) {
        expansion = csh_expand_vars(slash->argv[slash->argc - 1]);
        printf("%s\n", expansion);
        free(expansion);
    } else {
        expansion = csh_expand_vars(slash->argv[slash->argc - 1]);
		strcpy(slash->buffer, expansion);
        free(expansion);
		slash->length = strlen(slash->buffer);
        if(!quiet) {
            slash_refresh(slash, 1);
            printf("\n");
        }
		result = slash_execute(slash, slash->buffer);
    }
    optparse_del(parser);
	return result;
}
slash_command_completer(var_expand, slash_var_expand, env_var_ref_completer, "[-e] [-q] INPUT", "Display the given INPUT string with references to defined variables expanded.");
