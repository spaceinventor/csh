#include <stdio.h>
#include <stdlib.h>
#include <csp/csp.h>
#include <vmem/vmem.h>
#include <vmem/vmem_file.h>

#define MAX_FILE_SIZE 10000000
#define FILE_NAME "file.txt"

int8_t _file_init = 0;
void file_init_cb(param_t* param, int index);
char _file_name[128] = FILE_NAME;
uint32_t _file_size = MAX_FILE_SIZE;

/* Set the file name and path, relative to working directory */
PARAM_DEFINE_STATIC_RAM(302, file_name, PARAM_TYPE_STRING, sizeof(_file_name), 0, PM_CONF, NULL, NULL, &_file_name, NULL);

/* Set the file size of the target (or source) file */
PARAM_DEFINE_STATIC_RAM(303, file_size, PARAM_TYPE_UINT32, 1, 0, PM_CONF, NULL, NULL, &_file_size, NULL);

/* A write to this parameter initialize a file with properties according to the above parameters */
PARAM_DEFINE_STATIC_RAM(301, file_init, PARAM_TYPE_INT8, 1, 0, PM_CONF, file_init_cb, NULL, &_file_init, NULL);

/* This macro prepare the application to handle a file as a VMEM area, making it possible to read and write 
   to the file using the CSH commands 'upload' and 'download', after getting it's virtual address using
   the command 'vmem'.
   This macro allocate a memory area containing a copy of the file, for increased performance when reading.
   If this is not desired, a copy of vmem_file.c and vmem_file.h without the memory copy can be implemented
   in the user application. */
VMEM_DEFINE_FILE(file, "file", FILE_NAME, MAX_FILE_SIZE);

void file_init_cb(param_t* param, int index) {

    if (_file_size > MAX_FILE_SIZE) {
        /* File size bigger than maximum allowed for this application */
        param_set_int8(param, -1); 
        return;
    }
    vmem_file.size = _file_size;

    ((vmem_file_driver_t*)vmem_file.driver)->filename = _file_name;

    /* Create the file in case it does not already exist */
    FILE * stream = NULL;
	if ((stream = fopen(((vmem_file_driver_t *) vmem_file.driver)->filename, "a")) == NULL) {
        param_set_int8(param, -2); 
        return;
    } 
    fclose(stream);

    /* Initialize the file and associated memory area */
    vmem_file_init(&vmem_file);
}

/** 
   === User guide===

   # In CSH, run the commands
   set file_name myapp.exe
   set file_size 2485856
   set file_init 1

   get file_init
   # If the returned value is > 0, the requested file is initialized
   # and ready for read/write operations

   # Lists the available areas for read/write operations
   vmem

   # Use the VMEM address for the file returned by the previous command
   upload -v 2 myapp_local.exe 0x5598291432c0

   # If the connection times out or the complete file is not upload during the pass,
   # the upload can be continued by the command
   upload -v 2 -o 34744 myapp_local.exe 0x5598291432c0
   # which -o representing the number of bytes that is already successfully uploaded
   */