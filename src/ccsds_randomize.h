/*
 * ccsds_randomize.h
 *
 *  Created on: 25 nov 2022
 *  Author: Per Henrik Michaelsen
 */
#ifndef CCSDA_RANDOMIZE_H_
#define CCSDS_RANDOMIZE_H_

#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>

/**
 * Randomization is a mask xor'ed on a Reed-Solomon (255,223) block,
 * which is the data that comes after the ASM in a CADU frame.
 */

/**
 * @brief CCSDS randomize
 * Aplly the CCSDS randomization mask on a single Reed-Solomon block.
 * 
 * @param block The block
 */
void ccsds_randomize(uint8_t * block);

#endif // CCSDS_RANDOMIZE_H_
