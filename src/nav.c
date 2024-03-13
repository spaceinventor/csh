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

static int slash_cat(struct slash *slash) {

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

slash_command(cat, slash_cat, "[file]", "cat out file, no concat");