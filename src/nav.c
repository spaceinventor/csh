#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <slash/slash.h>
#include <slash/completer.h>

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
slash_command_completer(ls, slash_ls, slash_path_completer, "[path]", "list files");

static void print_cwd(void) {
    char *cwd = get_current_dir_name();
    if(NULL != cwd) {
        printf("%s\n", cwd);
        free(cwd);
    }
}

static int slash_pwd(struct slash *slash) {

    if (slash->argc != 0) {
        return SLASH_EUSAGE;
    }
    print_cwd();
	return SLASH_SUCCESS;
}
slash_command(pwd, slash_pwd, "", "Print current working directory");

static int slash_cd(struct slash *slash) {

    if (slash->argc != 2) {
        return SLASH_EUSAGE;
    }

    char *expanded_path = NULL;
    char *home = getenv("HOME");
    if(strlen(slash->argv[1])) {
        if (slash->argv[1][0] == '~') {
            expanded_path = calloc(PATH_MAX, sizeof(char));
            strcpy(expanded_path, home);
            if(slash->argv[1][1]) {
                strcat(expanded_path, &slash->argv[1][1]);
            }
            if (chdir(expanded_path) < 0) {
                printf("Failed to cd into %s\n", expanded_path);
                free(expanded_path);
                print_cwd();
                return SLASH_EINVAL;
            } else {
                free(expanded_path);
            }
        } else {
            if (chdir(slash->argv[1]) < 0) {
                printf("Failed to cd into %s\n", slash->argv[1]);
                print_cwd();
                return SLASH_EINVAL;
            }            
        }
    } else {
        if (chdir(home) < 0) {
            printf("Failed to cd into %s\n", home);
            print_cwd();
            return SLASH_EINVAL;
        }                   
    }
	return SLASH_SUCCESS;
}

slash_command_completer(cd, slash_cd, slash_path_completer, "", "change dir");

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

slash_command_completer(cat, slash_cat, slash_path_completer, "[file]", "cat out file, no concat");