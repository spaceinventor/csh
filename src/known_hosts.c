/**
 * Naive, slow and simple storage of nodeid and hostname
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <slash/slash.h>

void known_hosts_add(int find_host, char * new_name) {

    char * dirname = getenv("HOME");
    char oldpath[100];
    char newpath[100];

	if (strlen(dirname)) {
        snprintf(oldpath, 100, "%s/csh_hosts", dirname);
        snprintf(newpath, 100, "%s/csh_hosts.new", dirname);
    } else {
        snprintf(oldpath, 100, "csh_hosts");
        snprintf(newpath, 100, "csh_hosts.new");
    }

    /* Read from file */
	FILE * old = fopen(oldpath, "r");

    /* Read from file */
	FILE * new = fopen(newpath, "w");
	if (new == NULL) {
        if (old != NULL)
            fclose(old);
		return;
    }

    if (old != NULL) {
        int host;
        char name[20];
        while(fscanf(old, "%d,%20s\n", &host, name) == 2) {
            if (find_host != host) {
                fprintf(new, "%d,%s\n", host, name);
            }
        }
    }

    fprintf(new, "%d,%20s\n", find_host, new_name);

    if (old != NULL)
        fclose(old);
    
    fclose(new);

    remove(oldpath);
    rename(newpath, oldpath);

}

int known_hosts_get_name(int find_host, char * name, int buflen) {

    if (buflen < 20) {
        return 0;
    }

    char * dirname = getenv("HOME");
    char path[100];

	if (strlen(dirname)) {
        snprintf(path, 100, "%s/csh_hosts", dirname);
    } else {
        snprintf(path, 100, "csh_hosts");
    }

    /* Read from file */
	FILE * stream = fopen(path, "r");
	if (stream == NULL)
		return 0;

    int host;
    while(fscanf(stream, "%d,%20s\n", &host, name) == 2) {
        if (find_host == host) {
            fclose(stream);
            return 1;
        }
    }

    fclose(stream);
    return 0;

}


int known_hosts_get_node(char * find_name) {

    char * dirname = getenv("HOME");
    char path[100];

	if (strlen(dirname)) {
        snprintf(path, 100, "%s/csh_hosts", dirname);
    } else {
        snprintf(path, 100, "csh_hosts");
    }

    /* Read from file */
	FILE * stream = fopen(path, "r");
	if (stream == NULL)
		return 0;

    int host;
    char name[20];
    while(fscanf(stream, "%d,%20s\n", &host, name) == 2) {
        if (strcmp(find_name, name) == 0) {
            fclose(stream);
            return host;
        }
    }

    fclose(stream);
    return 0;

}


static int cmd_hosts(struct slash *slash)
{
	char * dirname = getenv("HOME");
    char path[100];

	if (strlen(dirname)) {
        snprintf(path, 100, "%s/csh_hosts", dirname);
    } else {
        snprintf(path, 100, "csh_hosts");
    }

    /* Read from file */
	FILE * stream = fopen(path, "r");
	if (stream == NULL)
		return 0;

    int host;
    char name[20];
    while(fscanf(stream, "%d,%20s\n", &host, name) == 2) {
        printf("%d\t%s\n", host, name);
    }

    fclose(stream);
    return SLASH_SUCCESS;
}

slash_command(hosts, cmd_hosts, NULL, NULL);
