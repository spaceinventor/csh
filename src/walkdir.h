/**
 * walkdir.h
 *  
 * Created: 22 Jun 2023
 * Author: Per Henrik Michaelsen
 */

/**
 * This module provides a generic function for walking a directory in a "left first" manner.
 * The function is recursive with each call doing the following:
 * 
 * The current state consists of 
 * - path : The path to the current entity, including the original path passes.
 * - depth: A counter that is decreased when the function is called.
 *          If this becomes zero the search does not process deeper but continues breath search.
 * Findings are reported by calls to directory and file callback funtions.
 * A void pointer, custom, is passed in each call to walkdir and to both callbackat functions.
 * This may be used to collect whatever information is relevant for the usage.
 *
 * At first call the path may refer to a file, in which case the file callback is called.
 * This is a degenerate case, as walkdir is otherwise only passed directories.
 * A directory is read and entries iterated.
 * When it is a file, the file callback is called.
 * When it is a directory, the following happens
 * - if depth has become zero, walkdir is not called and the walk stops along this path.
 * - the directory callback is called, and the return value determines if walkdir is to be called.
 * 
 * Names with first character being a dot ('.') are ignored.
 * This excludes current directory, parent directory and hidden entries.
 * 
 * Walkdir distinguishes between directories and regular files, while ignoring all other entrities,
 * like links and devices.    
 * The prototypes of the callback functions are
 *   bool dir_callback(const char * path, const char * last_entry, void * custom) 
 *   void file_callback(const char * path, const char * last_entry, void * custom)
 * The path does not include the last entry.
 */

#include <stddef.h>
#include <stdbool.h>

#define WALKDIR_MAX_ENTRIES 32
#define WALKDIR_MAX_PATH_SIZE 256

/**
 * @brief Walks a directory
 * 
 * @param path The path to be iterated.
 * @param path_size The maximum number of characters in path.
 * @param depth The depth counter.
 * @param dir_cb The directory callback function.
 * @param file_cb The file callback function.
 * @param custom A pointer that may be set to refer to any data structure used for aggregating
 *      information on the visited file system entities.
 * @param signum Pass a interrupt signal to stop walkdir
 */
void walkdir(char * path, size_t path_size, unsigned depth, 
 			  bool (*dir_cb)(const char *, const char *, void *), 
			  void (*file_cb)(const char *, const char *, void *), 
			  void * custom, int * signum);
