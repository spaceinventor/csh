/*
 * randombytes.c
 *
 *  Created on: 9. sep. 2020
 *      Author: johan
 */

#include <stdlib.h>
#include <time.h>

/* Required tweetnacl.c */
void randombytes(unsigned char * a, unsigned long long c) {
    // Note: Pseudo random since we are not initializing random!
    time_t t;
    srand((unsigned) time(&t) + rand());
    while(c > 0) {
        *a = rand() & 0xFF;
        a++;
        c--;
    }
}


