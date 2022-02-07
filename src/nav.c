#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <slash/slash.h>

static int slash_ls(struct slash *slash) {
    
    char cwd[100];
    char *ignore = getcwd(cwd, 100);
    (void) ignore;

    char cmd[100] = "ls";

    if (slash->argc == 2) {
        snprintf(cmd, 100, "ls %s", slash->argv[1]);
        if (slash->argv[1][0] == '/') {
            printf("%s:\n", slash->argv[1]);
        } else {
            printf("%s/%s:\n", cwd, slash->argv[1]);
        }
    } else {
        printf("%s:\n", cwd);
    }
   
    int ret = system(cmd);
    (void) ret;

	return SLASH_SUCCESS;
}

slash_command(ls, slash_ls, "[path]", "list files");


static int slash_cd(struct slash *slash) {

    if (slash->argc != 2) {
        return SLASH_EUSAGE;
    }

    if (chdir(slash->argv[1]) < 0) {
        return SLASH_EINVAL;
    }
	return SLASH_SUCCESS;
}

slash_command(cd, slash_cd, "", "change dir");
