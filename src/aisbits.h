/*
 * Copyright (c) 2016 Satlab ApS <satlab@satlab.com>
 * Proprietary software - All rights reserved.
 *
 * AIS bit-twiddling
 * FIXME: move to common lib
 */

#ifndef _AIS_BITS_H_
#define _AIS_BITS_H_

#include <stdint.h>
#include <stdbool.h>

#include <satlab/ais.h>

uint32_t get_mmsi(uint8_t *msg);
uint32_t ais_get_mmsi(ais_frame_t *frame);

uint8_t get_msgtype(uint8_t *data);
uint8_t ais_get_msgtype(ais_frame_t *frame);

uint8_t get_partnumber(uint8_t *data);
uint8_t ais_get_partnumber(ais_frame_t *frame);

int32_t get_long(uint8_t *data);
int32_t ais_get_lon(ais_frame_t *frame);

int32_t get_lat(uint8_t *data);
int32_t ais_get_lat(ais_frame_t *frame);

int get_name(uint8_t *data, char *name);
int ais_get_name(ais_frame_static_t *frame, char *name);

int get_destination(uint8_t *data, char *destination);
int ais_get_destination(ais_frame_static_t *frame, char *destination);

uint32_t get_offset_bits(uint8_t *data, int offset, int count);
uint8_t get_single_bit(uint8_t *data, int offset);

bool ais_has_position(ais_frame_t *frame);

#endif /* _AIS_BITS_H_ */
