#include <lua5.4/lua.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lualib.h>

#include <slash/slash.h>
#include <slash/optparse.h>
#include <string.h>
#include <stdio.h>

static lua_State * L = NULL;

static int lua_run_cmd(struct slash *slash) {

    if (L == NULL) {
        L = luaL_newstate();
        luaL_openlibs(L);
        printf("New lua state %p\n", L);
    }

    char * filename = NULL;
    optparse_t * parser = optparse_new("lua", "[command]");
    optparse_add_help(parser);
    optparse_add_string(parser, 'f', "file", "FILENAME", &filename, "File to store downloaded telemetry");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (filename != NULL) {
        FILE * file = fopen(filename, "a");
        if (file == NULL) {
            printf("File not found %s\n", filename);
            return SLASH_EINVAL;
        }
        fclose(file);
        luaL_dofile(L, filename);
        return SLASH_SUCCESS;
    }
    
    /* Build command string */
	char line[slash->line_size];
	line[0] = '\0';
	for (int arg = 1; arg < slash->argc; arg++) {
		strncat(line, slash->argv[arg], slash->line_size - strlen(line));
		strncat(line, " ", slash->line_size - strlen(line));
	}

    printf("Running %s\n", line);
    luaL_dostring(L, line);
    return SLASH_SUCCESS;
}

slash_command(lua, lua_run_cmd, NULL, NULL);
