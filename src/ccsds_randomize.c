/*
 * ccsds_randomize.c
 *
 *  Created on: 25 aug 2022
 *  Author: Per Henrik Michaelsen
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

static bool _initialized = 0;
static uint8_t _mask[255] = { 0 };

bool _check()
{
	// First 40 bit provided in CCSDS 131.0-B-4.
	// 1111 1111 0100 1000 0000 1110 1100 0000 1001 1010	
	uint8_t check[] = { 0xff, 0x48, 0x0e, 0xc0, 0x9a };
	bool equal = true;
	for (size_t i = 0; i < sizeof(check); ++i) {
		equal &= (_mask[i] == check[i]);
	}
	return equal;
}

static void _init()
{
    uint8_t lfsr = 0xff;
    uint8_t next = 0;
    uint8_t val = 0;
    size_t i = 0;
    size_t j = 0;
    for (i = 0; i < 255; ++i) {
        val = 0;
        for (j = 0; j < 8; ++j) {
            val = (val << 1) | (lfsr & 1);
            next = lfsr ^ (lfsr >> 3) ^ (lfsr >> 5) ^ (lfsr >> 7);
            lfsr = (lfsr >> 1) | ((next & 1) << 7);
        }
        _mask[i] = val;
    }

	if (!_check()) {
		printf("ERROR: CCSDS randomization first 40 bits are wrong.");
	}
}

void ccsds_randomize(uint8_t * block)
{
    if (!_initialized) {
        _init();
        _initialized = 1;
    }
    for (size_t i = 0; i < 255; ++i) {
        block[i] ^= _mask[i];
    }
}
