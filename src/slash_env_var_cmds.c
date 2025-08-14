#include <slash/slash.h>
#include <slash/optparse.h>
#include <stdlib.h>
#include <string.h>
#include <apm/environment.h>
#include "slash_env_var_completion.h"

/**
 * Adds the following commands to CSH:
 * - var set
 * - var get
 * - var unset
 * - var show
 * - var clear
 * - var expand
 * 
 * Examples:
 * 
 * var set WATCH_TIME 2000
 * var set CMD "list"
 * var show
 *
 * var expand "watch -n $(WATCH_TIME) $(CMD)"
 * - prints the expanded version of its argument
 * 
 * var expand -e "watch -n $(WATCH_TIME) $(CMD)"
 * -e means "csh-execute the expanded argument"
 * 
 * var unset CMD
 * var show
 * 
 * var clear
 * var show
 * 
 * var set MY_PASSWORD 123456 # Don't you ever do that!
 * var expand -q -e "command -p $(PASSWORD)"
 * -q means do NOT print the expanded version of its argument, but just csh-execute it.
 * This is useful if you do not want the "PASSWORD" variable to appear in logs, for instance. The log will still
 * contain this 'var expand -q -e "command -p $(PASSWORD)"', which is still useful enough for tracing/debugging without
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

static int prefix_length(const char *s1, const char *s2)
{
	int len = 0;
    if (s1 && s2) {
        while (*s1 && *s2 && *s1 == *s2) {
            len++;
            s1++;
            s2++;
        }
    }
	return len;
}


static void env_var_completer_csh_foreach_var_cb(const char *name, void *ctx) {
    env_var_completer_t *my_ctx = (env_var_completer_t *)ctx;
    size_t cur_cmd_len = strlen(name);
    size_t to_match_len = strlen(my_ctx->to_match);
    if(strncmp(my_ctx->to_match, name, slash_min(cur_cmd_len, to_match_len)) == 0) {
        my_ctx->matches++;
        if(NULL != my_ctx->previous_match) {
            my_ctx->prev_match_length = prefix_length(name, my_ctx->previous_match);
        } else {
            my_ctx->previous_match = name;
            my_ctx->prev_match_length = strlen(name);
        }
        switch(my_ctx->matches) {
            case 1:
                break;
            case 2:
                slash_printf(my_ctx->slash, "\n%s\n", my_ctx->previous_match);
                slash_printf(my_ctx->slash, "%s\n", name);
                break;
            default:
                slash_printf(my_ctx->slash, "%s\n", name);
                break;
        }
    } else {
        if(my_ctx->matches == 2) {
            my_ctx->matches++;
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
                            strcat(slash->buffer, " ");
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
        if(*token) {
            switch(ctx.matches) {
                case 0:
                    break;
                case 1: {
                    strcpy(slash->buffer + (ctx.slash_buffer_start - slash->buffer), ctx.previous_match);
                    strcat(slash->buffer, " ");
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
}


slash_command_group(env, "CSH environment variables");

const struct slash_command slash_cmd_var_set;
static int slash_var_set(struct slash *slash)
{
    optparse_t * parser = optparse_new_ex(slash_cmd_var_set.name, slash_cmd_var_set.args, slash_cmd_var_set.help);
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
slash_command_sub_completer(var, set, slash_var_set, env_var_completer, "NAME VALUE", "Create or update an environment variable in CSH");


const struct slash_command slash_cmd_var_unset;
static int slash_var_unset(struct slash *slash)
{
    optparse_t * parser = optparse_new_ex(slash_cmd_var_unset.name, slash_cmd_var_unset.args, slash_cmd_var_unset.help);
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
slash_command_sub_completer(var, unset, slash_var_unset, env_var_completer, "NAME", "Remove an environment variable in CSH");

const struct slash_command slash_cmd_var_clear;
static int slash_var_clear(struct slash *slash)
{
    optparse_t * parser = optparse_new_ex(slash_cmd_var_clear.name, slash_cmd_var_clear.args, slash_cmd_var_clear.help);
    optparse_add_help(parser);
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi == -1) {
        optparse_del(parser);
        return SLASH_EINVAL;
    }
    if ((slash->argc - argi) > 1) {
        printf("var clear takes no parameters\n");
        optparse_del(parser);
	    return SLASH_EINVAL;
    }
    csh_clearenv();
    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(var, clear, slash_var_clear, NULL, "Clear all environment variables in CSH");

void env_var_ref_completer(struct slash * slash, char * token) {
    int length = strlen(token);
    char *var_start;
    char *var_end;
    bool closing_bracket = false;
    if(length > 1) {
        /* All of this is only relevant if token is at least "$(", as there is nothing to complete otherwise */
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
                                strcat(slash->buffer, ") ");
                            }
                            break;
                            default: {
                                strncpy(slash->buffer + (ctx.slash_buffer_start - slash->buffer), ctx.previous_match, ctx.prev_match_length);
                                char *last_copied_char = slash->buffer + (ctx.slash_buffer_start - slash->buffer) + ctx.prev_match_length;
                                *last_copied_char = '\0';
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
}

const struct slash_command slash_cmd_var_get;
static int slash_var_get(struct slash *slash)
{
    optparse_t * parser = optparse_new_ex(slash_cmd_var_get.name, slash_cmd_var_get.args, slash_cmd_var_get.help);
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
slash_command_sub_completer(var, get, slash_var_get, env_var_completer, "NAME", "Show the value of the given 'NAME' environment variable in CSH, shows nothing if variable is not defined");

static void print_var(const char *name, void *ctx) {
    (void)ctx;
    printf("%s=%s\n", name, csh_getvar(name));
}

const struct slash_command slash_cmd_var_show;
static int slash_var_show(struct slash *slash)
{
    optparse_t * parser = optparse_new_ex(slash_cmd_var_show.name, slash_cmd_var_show.args, slash_cmd_var_show.help);
    optparse_add_help(parser);
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi == -1) {
        optparse_del(parser);
        return SLASH_EINVAL;
    }
    if ((slash->argc - argi) > 1) {
        printf("var show takes no parameters\n");
        optparse_del(parser);
	    return SLASH_EINVAL;
    }
    csh_foreach_var(print_var, NULL);
    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(var, show, slash_var_show, NULL, "Print the defined variables and their values");

const struct slash_command slash_cmd_var_expand;
static int slash_var_expand(struct slash *slash)
{
    int result = SLASH_SUCCESS;
    int execute = 0;
    int quiet = 0;
    optparse_t * parser = optparse_new_ex(slash_cmd_var_expand.name, slash_cmd_var_expand.args, slash_cmd_var_expand.help);
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
slash_command_sub_completer(var, expand, slash_var_expand, env_var_ref_completer, "[-e] [-q] INPUT", "Display the given INPUT string with references to defined variables expanded.");
