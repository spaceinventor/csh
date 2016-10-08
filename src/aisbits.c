/*
 * Copyright (c) 2016 Satlab ApS <satlab@satlab.com>
 * Proprietary software - All rights reserved.
 *
 * AIS bit-twiddling
 * FIXME: move to common lib
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>

#include <satlab/ais.h>

uint32_t get_offset_bits(uint8_t *data, int offset, int count)
{
	int shft;
	uint32_t ret, byte;

	byte = offset / 8;
	shft = offset & 7;

	ret = (((uint32_t)data[byte + 0]) << (shft + 24));

	if (count + shft > 8)
		ret |= (((uint32_t)data[byte + 1]) << (shft + 16));
	if (count + shft > 16)
		ret |= (((uint32_t)data[byte + 2]) << (shft + 8));
	if (count + shft > 24)
		ret |= (((uint32_t)data[byte + 3]) << (shft + 0));
	if (count + shft > 32)
		ret |= (((uint32_t)data[byte + 4]) << (shft - 8));

	return ret >> (32 - count);
}

char convert_6bit_ascii(char c)
{
	char conv[] = {'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
		       'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U',
		       'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '-', ' ',
		       '!', '\"', '#', '$', '%', '&', '\'', '(', ')', '*', '+',
		       ',', '-', '.', '/', '0', '1', '2', '3', '4', '5', '6',
		       '7', '8', '9', ':', ';', '<', '=', '>', '?'};
	if (c < 0 || c > sizeof(conv))
		return '@';

	return conv[(int)c];
}

uint8_t get_msgtype(uint8_t *data)
{
	return (data[0] >> 2) & 0x3F;
}

uint8_t ais_get_msgtype(ais_frame_t *frame)
{
	return get_msgtype(frame->data);
}

uint8_t get_partnumber(uint8_t *data)
{
	return get_offset_bits(data, 38, 2);
}

uint8_t ais_get_partnumber(ais_frame_t *frame)
{
	return get_partnumber(frame->data);
}

uint32_t get_mmsi(uint8_t *data)
{
	return get_offset_bits(data, 8, 30);
}

uint32_t ais_get_mmsi(ais_frame_t *frame)
{
	return get_mmsi(frame->data);
}

int32_t get_lat(uint8_t *data)
{
	uint8_t id;
	uint32_t ulat = 0;
	int32_t lat = 0;
	id = get_msgtype(data);

	if (1 <= id && id < 4)
		ulat = (get_offset_bits(data,89,27));
	else if (id == 4) 
		ulat = get_offset_bits(data,107,27);
	else if ((id == 18) || (id == 19))
		ulat = get_offset_bits(data,85,27);
	else if (id == 27)
		ulat = get_offset_bits(data,44+18,17);
	else 
		ulat = 0x3412140; /* +91, M.1371-5 default value */

	/* Sign extension */
	if (id == 27)
		lat = (((int32_t) (ulat<<15))>>15) * 1000;
	else
		lat = (((int32_t) (ulat<<5))>>5);


	return lat;
}

int32_t ais_get_lat(ais_frame_t *frame)
{
	return get_lat(frame->data);
}

int32_t get_lon(uint8_t *data)
{
	uint8_t id;
	uint32_t ulon = 0;
	int32_t lon = 0;
	id = get_msgtype(data);

	if (1 <= id && id < 4)
		ulon = (get_offset_bits(data,61,28));
	else if (id == 4) 
		ulon = get_offset_bits(data,79,28);
	else if ((id == 18) || (id == 19))
		ulon = get_offset_bits(data,57,28);
	else if (id == 27)
		ulon = get_offset_bits(data,44,18);
	else 
		ulon = 0x6791AC0; /* +181, M.1371-5 default value */

	/* Sign extension */
	if (id == 27)
		lon = (((int32_t) (ulon<<14))>>14) * 1000;
	else
		lon = (((int32_t) (ulon<<4))>>4);

	return lon;
}

int32_t ais_get_lon(ais_frame_t *frame)
{
	return get_lon(frame->data);
}

int get_name(uint8_t *data, char *name)
{
	int i;
	char c;
	unsigned int start, type;

	type = get_msgtype(data);

	if (type == 5)
		start = 112;
	else if (type == 24 && get_partnumber(data) == 0)
		start = 40;
	else
		return -EINVAL;

	for (i = 0; i < 20; i++) {
		name[i] = '\0';
		c = get_offset_bits(data, start+6*i, 6);
		if (!c)
			break;
		name[i] = convert_6bit_ascii(c);
	}
	name[20] = '\0';

	return 0;
}

int ais_get_name(ais_frame_static_t *frame, char *name)
{
	return get_name(frame->data, name);
}

int get_destination(uint8_t *data, char *destination)
{
	int i;
	char c;

	if (get_msgtype(data) != 5)
		return -EINVAL;

	for (i = 0; i < 20; i++) {
		destination[i] = '\0';
		c = get_offset_bits(data, 302+6*i, 6);
		if (!c)
			break;
		destination[i] = convert_6bit_ascii(c);
	}
	destination[20] = '\0';

	return 0;
}

int ais_get_destination(ais_frame_static_t *frame, char *destination)
{
	return get_destination(frame->data, destination);
}

bool ais_has_position(ais_frame_t *frame)
{
	uint8_t msgid = ais_get_msgtype(frame);
	return (msgid >= 1 && msgid <=4) || msgid==18 || msgid==19 || msgid == 27;
}
