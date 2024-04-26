#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <slash/slash.h>
#include <slash/completer.h>

static int slash_ls(slash_t *slash) {
    
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

slash_command_completer(ls, slash_ls, slash_path_completer, "[path]", "list files");


static int slash_cd(slash_t *slash) {

    if (slash->argc != 2) {
        return SLASH_EUSAGE;
    }

    if (chdir(slash->argv[1]) < 0) {
        return SLASH_EINVAL;
    }
	return SLASH_SUCCESS;
}

slash_command_completer(cd, slash_cd, slash_path_completer, "", "change dir");

static int slash_cat(slash_t *slash) {

    if (slash->argc != 2) {
        return SLASH_EUSAGE;
    }

    FILE *fp;
    char buffer[512];

    fp = fopen(slash->argv[1], "r");

    if (fp == NULL) {
        printf("Failed to open the file %s\n", slash->argv[1]);
        return SLASH_EINVAL;
    }

    while (fgets(buffer, 512, fp) != NULL) {
        printf("%s", buffer);
    }

    fclose(fp);

	return SLASH_SUCCESS;
}

slash_command_completer(cat, slash_cat, slash_path_completer, "[file]", "cat out file, no concat");