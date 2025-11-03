/**
 * walkdir.c
 *  
 * Created: 22 Jun 2023
 * Author: Per Henrik Michaelsen
 */

#include "walkdir.h"

#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>

void walkdir(char * path, size_t path_size, unsigned depth, 
 			  bool (*dir_cb)(const char *, const char *, void *), 
			  void (*file_cb)(const char *, const char *, void *), 
			  void * custom, int * signal)
{
    DIR * p = opendir(path);
	if (p == 0) {
        if (errno == ENOTDIR) {
            // It exists but is not a directory
            file_cb(path, path, custom);
        }
		return;
	}
	
    struct dirent * entry;
	struct stat stat_info;

    while ((entry = readdir(p)) != NULL) {
		// Save path length
		size_t path_len = strlen(path);
		if(path[path_len - 1] != '/') {
			strncat(path, "/", path_size - path_len);
		}
		strncat(path, entry->d_name, path_size - strlen(path));

		// entry->d_type is not set on some file systems, like sshfs mount of si
		lstat(path, &stat_info);
		bool isdir = S_ISDIR(stat_info.st_mode);
		bool isfile = S_ISREG(stat_info.st_mode);

  		// Ignore '.', '..' and hidden entries
        if (((isdir && depth) || isfile) && (entry->d_name[0] != '.')) {
        	if (isdir) {
				if (dir_cb && dir_cb(path, entry->d_name, custom)) {
					walkdir(path, path_size, depth-1, dir_cb, file_cb, custom, signal);
				}
			} 
			else {
				if (file_cb) {
					file_cb(path, entry->d_name, custom);
				}
			}
		}
		if(signal && *signal == SIGINT){
			break;
		}

		// Restore path length
		path[path_len] = 0;
    }
    closedir(p);
}
