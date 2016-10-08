/*
 * Copyright (c) 2014 Satlab ApS <satlab@satlab.com>
 * Proprietary software - All rights reserved.
 *
 * Commands for AIS subsystem
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>

#include <csp/csp.h>
#include <csp/csp_endian.h>
#include <slash/slash.h>
#include <satlab/ais.h>

#include "aisbits.h"

static struct timespec timespec_diff(struct timespec start, struct timespec end)
{
	struct timespec temp;

	if (end.tv_nsec - start.tv_nsec < 0) {
		temp.tv_sec = end.tv_sec - start.tv_sec - 1;
		temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec - start.tv_sec;
		temp.tv_nsec = end.tv_nsec - start.tv_nsec;
	}

	return temp;
}

/* Default node and timeout */
static uint8_t ais_node = 22;
static uint32_t ais_timeout = 1000;

static int8_t convert_rssi(uint8_t rssi)
{
	return -140 + (rssi/2);
}

static int frame_callback(ais_frame_t *frame)
{
	int i;

	printf("Frame %u (MMSI %09u, type %2u, ch%u): ",
	       frame->seq_nr, ais_get_mmsi(frame), ais_get_msgtype(frame),
	       (frame->flags >> 1) + 1);
	for (i = 0; i < 23; i++)
		printf("%02x", frame->data[i]);
	printf("\n");

	return 0;
}

static int short_callback(uint32_t seq_nr, ais_short_frame_t *frame)
{
	printf("Frame %u (MMSI %09u, type %2u, ch%u)\n",
	       seq_nr, frame->mmsi, frame->msgid,
	       (frame->flags >> 1) + 1);

	return 0;
}

static int static_callback(ais_frame_static_t *frame)
{
	char name[21], destination[21];

	if (ais_get_name(frame, name))
		sprintf(name, "-NO NAME-");
	if (ais_get_destination(frame, destination))
		sprintf(destination, "-NO DESTINATION-");

	printf("Frame %u (type %02u): '%-20s' on the way to '%-20s'\n",
	       frame->seq_nr, get_msgtype(frame->data), name, destination);
	return 0;
}

static const char *mode_to_string(uint8_t mode)
{
	switch (mode) {
	case AIS2_MODE_IDLE:
		return "idle";
	case AIS2_MODE_RUNNING_SINGLE:
		return "single";
	case AIS2_MODE_RUNNING_CONT:
		return "continue";
	case AIS2_MODE_RUNNING_AND_SAVING_SINGLE:
		return "single-save";
	case AIS2_MODE_RUNNING_AND_SAVING_CONT:
		return "continue-save";
	default:
		return "unknown";
	}
	return 0;
}

static const char *storage_to_string(uint8_t storage)
{
	switch (storage) {
	case AIS_STORAGE_ID_SDCARD:
		return "sd-card";
	case AIS_STORAGE_ID_NORFLASH:
		return "nor-flash";
	default:
		return "unknown";
	}
}

static bool string_to_bool(char *arg)
{
	if (*arg == '1' || *arg == 'y' || !strcmp(arg, "true"))
		return true;

	return false;
}

slash_command_group(ais, "AIS subsystem commands");

static int cmd_ais_node(struct slash *slash)
{
	uint8_t node;
	char *endptr;

	if (slash->argc < 2) {
		printf("AIS node address: %hhu\n", ais_node);
		return 0;
	}

	node = (uint8_t)strtoul(slash->argv[1], &endptr, 10);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	ais_node = node;

	return 0;
}
slash_command_sub(ais, node, cmd_ais_node, "<address>",
		  "Set/read AIS node address");

static int cmd_ais_timeout(struct slash *slash)
{
	uint32_t timeout;
	char *endptr;

	if (slash->argc < 2) {
		printf("AIS timeout: %u\n", ais_timeout);
		return 0;
	}

	timeout = (uint32_t)strtoul(slash->argv[1], &endptr, 10);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	ais_timeout = timeout;

	return 0;
}
slash_command_sub(ais, timeout, cmd_ais_timeout, "<ms>",
		  "Set/read AIS communication timeout");

static int cmd_ais_status(struct slash *slash)
{
	ais2_port_status_t status;

	if (ais_get_status(ais_node, ais_timeout, &status)) {
		printf("Failed to get status\n");
		return -ENOENT;
	}

	printf("Errors:           %"PRIu8"\n", status.critical_errors);
	printf("Warning:          %"PRIu8"\n", status.warnings);
	printf("Samples:          %"PRIu16"\n", status.runs);
	printf("Packets Detected: %"PRIu16"\n", status.packs_detected);
	printf("Running From:     %"PRIu8"\n", status.running_from);
	printf("Bootcount:        %"PRIu16"\n", status.bootcount);
	printf("CRC OK:           %"PRIu32"\n", status.crc_ok);
	printf("Unique MMSI:      %"PRIu16"\n", status.unique_mmsi);
	printf("Latest MMSI:      %09"PRIu32"\n", status.latest_mmsi);
	printf("Latest Longitude: %"PRIi32"\n", status.latest_long);
	printf("Latest Latitude:  %"PRIi32"\n", status.latest_lat);
	printf("RSSI:             %"PRIi8" dBm\n", convert_rssi(status.rssi));
	printf("Flags:            %"PRIu8"\n", status.flags);

	return 0;
}
slash_command_sub(ais, status, cmd_ais_status, NULL, "Read status");

slash_command_subgroup(ais, store, "Message stores subcommands");

static int cmd_ais_store_read(struct slash *slash)
{
	printf("Dynamic store:\n");
	ais_get_frames(ais_node, ais_timeout, AIS_BACKEND_FILE,
		       0, 8, &frame_callback);

	printf("Static store:\n");
	ais_get_static_frames(ais_node, ais_timeout, AIS_BACKEND_STATIC,
			      0, 8, &static_callback);

	return 0;
}
slash_command_subsub(ais, store, read, cmd_ais_store_read, NULL,
		     "Read message stores");

static int string_to_backend(char *arg, uint8_t *backend)
{
	char *endptr;

	if (!strcmp(arg, "file")) {
		*backend = AIS_BACKEND_FILE;
	} else if (!strcmp(arg, "unique")) {
		*backend = AIS_BACKEND_UNIQUE;
	} else if (!strcmp(arg, "static")) {
		*backend = AIS_BACKEND_STATIC;
	} else {
		*backend = (uint8_t)strtoul(arg, &endptr, 10);
		if (*endptr != '\0')
			return -EINVAL;
	}

	return 0;
}

static int cmd_ais_store_request(struct slash *slash)
{
	int ret;
	uint32_t start;
	uint8_t backend;
	/* FIXME: allow bitmap and frames to be set */
	uint8_t bitmap[AIS_PORT_STORE_REQUEST_BITMAP_SIZE] = {0xAA, 0xAA};
	uint8_t frames_per_reply = 2;
	bool short_frames = false;
	char *endptr;

	if (slash->argc < 3)
		return SLASH_EUSAGE;

	if (string_to_backend(slash->argv[1], &backend) < 0)
		return SLASH_EUSAGE;

	start = (uint32_t)strtoul(slash->argv[2], &endptr, 10);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	if (slash->argc > 3)
		short_frames = string_to_bool(slash->argv[3]);

	if (backend == AIS_BACKEND_FILE || backend == AIS_BACKEND_UNIQUE) {
		if (short_frames) {
			ret = ais_request_bitmap_short(ais_node, ais_timeout, backend, start,
						       frames_per_reply, bitmap, &short_callback);
		} else {
			ret = ais_request_bitmap_frames(ais_node, ais_timeout, backend, start,
							frames_per_reply, bitmap, &frame_callback);
		}
	} else {
		ret = ais_request_bitmap_static(ais_node, ais_timeout, backend, start,
						frames_per_reply, bitmap, &static_callback);
	}
	if (ret < 0) {
		printf("error: %s\n", strerror(-ret));
		return ret;
	}

	return 0;
}
slash_command_subsub(ais, store, request, cmd_ais_store_request,
		     "<backend> <start> [short]",
		     "Request from message stores");

static int cmd_ais_store_status(struct slash *slash)
{
	int ret;
	uint8_t backend;
	uint32_t size, count, seq;

	if (slash->argc < 2)
		return SLASH_EUSAGE;

	if (string_to_backend(slash->argv[1], &backend) < 0)
		return SLASH_EUSAGE;

	ret = ais_get_store_status(ais_node, ais_timeout, backend,
				   &size, &count, &seq);
	if (ret < 0) {
		printf("error: %s\n", strerror(-ret));
		return ret;
	}

	printf("size:  %"PRIu32"\n", size);
	printf("count: %"PRIu32"\n", count);
	printf("seq:   %"PRIu32"\n", seq);

	return 0;
}
slash_command_subsub(ais, store, status, cmd_ais_store_status,
		     "<backend>", "Get store status");

static int cmd_ais_store_size(struct slash *slash)
{
	int ret;
	uint8_t backend;
	uint32_t size;
	char *endptr;

	if (slash->argc < 3)
		return SLASH_EUSAGE;

	if (string_to_backend(slash->argv[1], &backend) < 0)
		return SLASH_EUSAGE;

	size = (uint32_t)strtoul(slash->argv[2], &endptr, 10);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	ret = ais_set_store_size(ais_node, ais_timeout, backend, size);
	if (ret < 0) {
		printf("error: %s\n", strerror(-ret));
		return ret;
	}

	return 0;
}
slash_command_subsub(ais, store, size, cmd_ais_store_size,
		     "<backend> <size>", "Set store size");

static int cmd_ais_mode(struct slash *slash)
{
	int ret;
	uint8_t mode, storage = 0, name = 0, samples = 0;

	/* Request current setting if no argument given */
	if (slash->argc < 2) {
		ret = ais_get_mode(ais_node, ais_timeout,
				   &mode, &storage,
				   &name, &samples);
		if (ret < 0) {
			printf("error: %s\n", strerror(ret));
			return ret;
		}

		printf("Mode:    %s\n", mode_to_string(mode));
		printf("Storage: %s\n", storage_to_string(storage));
		printf("Name:    %"PRIu8"\n", name);
		printf("Samples: %"PRIu8"\n", samples);

		return ret;
	}

	if (!strcmp(slash->argv[1], "idle")) {
		mode = AIS2_MODE_IDLE;
	} else if (!strcmp(slash->argv[1], "single")) {
		mode = AIS2_MODE_RUNNING_SINGLE;
	} else if (!strcmp(slash->argv[1], "continue")) {
		if (slash->argc < 3)
			return SLASH_EUSAGE;
		mode = AIS2_MODE_RUNNING_CONT;
		samples = atoi(slash->argv[2]);
	} else if (!strcmp(slash->argv[1], "single-save")) {
		if (slash->argc < 5)
			return SLASH_EUSAGE;
		mode = AIS2_MODE_RUNNING_AND_SAVING_SINGLE;
		samples = atoi(slash->argv[2]);
		name = atoi(slash->argv[4]);
		if (!strcmp(slash->argv[3], "sd")) {
			storage = 0;
		} else if (!strcmp(slash->argv[3], "nor")) {
			storage = 1;
		} else {
			printf("unknown storage\n");
			return -1;
		}
	} else if (!strcmp(slash->argv[1], "continue-save")) {
		if (slash->argc < 5)
			return SLASH_EUSAGE;
		mode = AIS2_MODE_RUNNING_AND_SAVING_CONT;
		samples = atoi(slash->argv[2]);
		name = atoi(slash->argv[4]);
		if (!strcmp(slash->argv[3], "sd")) {
			storage = 0;
		} else if (!strcmp(slash->argv[3], "nor")) {
			storage = 1;
		} else {
			printf("unknown storage\n");
			return -1;
		}
	} else {
		printf("unknown mode: %s\n", slash->argv[1]);
		return -1;
	}

	ret = ais_set_mode(ais_node, ais_timeout, mode, storage, name, samples);
	if (ret < 0)
		printf("error: %s\n", strerror(ret));

	return ret;
}
slash_command_sub(ais, mode, cmd_ais_mode,
		  "[mode] [samples] [storage] [name]",
		  "Set receiver mode");

static int cmd_ais_config(struct slash *slash)
{
	int ret;
	uint16_t boots;
	uint8_t adc_gain, filter_gain, lna_gain, autostart;
	char *endptr;

	/* Request current setting if no argument given */
	if (slash->argc < 6) {
		ret = ais_get_config(ais_node, ais_timeout, &boots,
				     &adc_gain, &filter_gain,
				     &lna_gain, &autostart);
		if (ret < 0) {
			printf("error: %s\n", strerror(ret));
			return ret;
		}

		printf("Boots:       %"PRIu16"\n", boots);
		printf("ADC gain:    %"PRIu8"\n", adc_gain);
		printf("Filter gain: %"PRIu8"\n", filter_gain);
		printf("LNA gain:    %"PRIu8"\n", lna_gain);
		printf("Autostart:   %"PRIu8"\n", autostart);

		return ret;
	}

	boots = (uint16_t)strtoul(slash->argv[1], &endptr, 10);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	adc_gain = (uint8_t)strtoul(slash->argv[2], &endptr, 10);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	filter_gain = (uint8_t)strtoul(slash->argv[3], &endptr, 10);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	lna_gain = (uint8_t)strtoul(slash->argv[4], &endptr, 10);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	autostart = (uint8_t)strtoul(slash->argv[5], &endptr, 10);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	ret = ais_set_config(ais_node, ais_timeout, boots,
			     adc_gain, filter_gain,
			     lna_gain, autostart);
	if (ret < 0)
		printf("error: %s\n", strerror(ret));

	return ret;
}
slash_command_sub(ais, config, cmd_ais_config,
		  "[<boots> <adc-gain> <filter-gain> <lna-gain> <autostart>]",
		  "Set receiver configuration");

slash_command_subgroup(ais, time, "Time subcommands");

static int cmd_ais_time_set(struct slash *slash)
{
	int ret;
	uint32_t sec, nsec = 0;
	char *endptr;
	struct timespec now;

	if (slash->argc < 2)
		return SLASH_EUSAGE;

	if (!strcmp("auto", slash->argv[1])) {
		clock_gettime(CLOCK_REALTIME, &now);
		sec = now.tv_sec;
		nsec = now.tv_nsec;
	} else {
		sec = (uint32_t)strtoul(slash->argv[1], &endptr, 10);
		if (*endptr != '\0')
			return SLASH_EUSAGE;

		if (slash->argc > 2) {
			nsec = (uint8_t)strtoul(slash->argv[2], &endptr, 10);
			if (*endptr != '\0')
				return SLASH_EUSAGE;
		}
	}

	ret = ais_set_time(ais_node, ais_timeout, sec, nsec);
	if (ret < 0)
		printf("error: %s\n", strerror(ret));

	return ret;
}
slash_command_subsub(ais, time, set, cmd_ais_time_set,
		     "<sec|auto> [nsec]",
		     "Set receiver time");

static int cmd_ais_time_get(struct slash *slash)
{
	int ret;
	uint32_t sec, nsec;

	ret = ais_get_time(ais_node, ais_timeout, &sec, &nsec);
	if (ret < 0) {
		printf("error: %s\n", strerror(ret));
		return ret;
	}

	printf("current time: %9"PRIu32".%09"PRIu32"\n", sec, nsec);

	return ret;
}
slash_command_subsub(ais, time, get, cmd_ais_time_get, NULL,
		     "Get receiver time");

static int cmd_ais_time_loop(struct slash *slash)
{
	int ret = 0;
	uint32_t ais_sec, ais_nsec;
	struct timespec ais_time, our_time, diff;

	while (slash_getchar_nonblock(slash) < 0) {
		/* Get our time */
		ret = clock_gettime(CLOCK_REALTIME, &our_time);
		if (ret < 0) {
			printf("error: %s\n", strerror(ret));
			return ret;
		}

		/* Get receiver time */
		ret = ais_get_time(ais_node, ais_timeout, &ais_sec, &ais_nsec);
		if (ret < 0) {
			printf("error: %s\n", strerror(ret));
			return ret;
		}

		ais_time.tv_sec = ais_sec;
		ais_time.tv_nsec = ais_nsec;

		diff = timespec_diff(ais_time, our_time);

		printf("AIS time: %10"PRIu32".%09"PRIu32"\n", ais_sec, ais_nsec);
		printf("OUR time: %10"PRIu32".%09"PRIu32"\n", (uint32_t) our_time.tv_sec, (uint32_t) our_time.tv_nsec);
		printf("DIFF:     %10"PRIu32".%09"PRIu32"\n", (uint32_t) diff.tv_sec, (uint32_t) diff.tv_nsec);

		sleep(1);
	}

	return ret;
}
slash_command_subsub(ais, time, loop, cmd_ais_time_loop, NULL,
		     "Read receiver time in loop");

static int cmd_ais_reboot(struct slash *slash)
{
	csp_reboot(ais_node);

	return 0;
}
slash_command_sub(ais, reboot, cmd_ais_reboot, NULL, "Reboot AIS receiver");

static int cmd_ais_ping(struct slash *slash)
{
	int ret = csp_ping(ais_node, ais_timeout, 1, CSP_O_NONE);

	if (ret < 0) {
		printf("error: timeout\n");
		return ret;
	}

	printf("Reply in %d ms\n", ret);

	return 0;
}
slash_command_sub(ais, ping, cmd_ais_ping, NULL, "Ping AIS receiver");

static int cmd_ais_demod(struct slash *slash)
{
	return -ENOSYS;
}
slash_command_sub(ais, demod, cmd_ais_demod, NULL, "Demodulator setup");

static int cmd_ais_demod_user(struct slash *slash)
{
	int ret;
	uint8_t type = 0;
	uint32_t samples;

	if (slash->argc > 1)
		type = atoi(slash->argv[1]);

	ret = ais_demod_user(ais_node, ais_timeout, (uint8_t *)&type, sizeof(type), (void *)&samples, sizeof(samples));
	if (ret < 0) {
		printf("error: failed to read user data: %d\n", ret);
		return -1;
	}

	printf("received: %u\n", (unsigned)csp_ntoh32(samples));

	return 0;
}
slash_command_subsub(ais, demod, user, cmd_ais_demod_user, NULL, "Demodulator user setup");

static int cmd_ais_demod_set(struct slash *slash)
{
	int ret;
	char *path;
	uint16_t boots = 1;

	if (slash->argc < 2)
		return SLASH_EUSAGE;

	path = slash->argv[1];

	if (slash->argc > 2)
		boots = atoi(slash->argv[2]);

	if (boots < 1)
		return SLASH_EUSAGE;

	ret = ais_demod_set(ais_node, ais_timeout, path, boots);
	if (ret < 0) {
		printf("error: failed to read user data: %d\n", ret);
		return -1;
	}

	return 0;
}
slash_command_subsub(ais, demod, set, cmd_ais_demod_set, "<path> [boots]",
		     "Demodulator set");

static int cmd_ais_demod_get(struct slash *slash)
{
	int ret;
	char path[128];
	uint16_t boots;

	ret = ais_demod_get(ais_node, ais_timeout, path, &boots);
	if (ret < 0) {
		printf("error: failed to read user data: %d\n", ret);
		return -1;
	}

	printf("boots: %hu\n", boots);
	printf("path:  %s\n", path);

	return 0;
}
slash_command_subsub(ais, demod, get, cmd_ais_demod_get, NULL,
		     "Demodulator get");

slash_command_subgroup(ais, channels, "Channel subcommands");

static int cmd_ais_channels_set(struct slash *slash)
{
	int ret;
	uint8_t channels;

	if (slash->argc < 2)
		return SLASH_EUSAGE;

	if (!strcmp(slash->argv[1], "ch12")) {
		channels = AIS_PORT_CHANNEL_CH1_2;
	} else if (!strcmp(slash->argv[1], "ch34")) {
		channels = AIS_PORT_CHANNEL_CH3_4;
	} else {
		printf("unknown channels: %s\n", slash->argv[1]);
		return -1;
	}

	ret = ais_channels_set(ais_node, ais_timeout, channels, 0);
	if (ret < 0) {
		printf("error: failed to set channels: %d\n", ret);
		return -1;
	}

	return 0;
}
slash_command_subsub(ais, channels, set, cmd_ais_channels_set, "<channels>",
		     "Set channels (ch12 or ch34)");

static int cmd_ais_channels_get(struct slash *slash)
{
	int ret;
	uint8_t channels;
	char *chstr = "unknown";

	ret = ais_channels_get(ais_node, ais_timeout, &channels);
	if (ret < 0) {
		printf("error: failed to get channels: %d\n", ret);
		return -1;
	}

	if (channels == AIS_PORT_CHANNEL_CH1_2) {
		chstr = "ch12";
	} else if (channels == AIS_PORT_CHANNEL_CH3_4) {
		chstr = "ch34";
	}

	printf("channels: %s\n", chstr);

	return 0;
}
slash_command_subsub(ais, channels, get, cmd_ais_channels_get, NULL,
		     "Get channels");

static int cmd_ais_version(struct slash *slash)
{
	int ret;
	char version[6];

	ret = ais_get_version(ais_node, ais_timeout, version);
	if (ret < 0) {
		printf("error: failed to get version: %d\n", ret);
		return -1;
	}

	version[sizeof(version) - 1] = '\0';

	printf("version: %s\n", version);

	return 0;
}
slash_command_sub(ais, version, cmd_ais_version, NULL,
		  "Get firmware version");

slash_command_subgroup(ais, eth, "Ethernet subcommands");

static int cmd_ais_eth_set(struct slash *slash)
{
	int ret;
	char *endptr;
	uint8_t enable;
	uint16_t boots = 1;

	if (slash->argc < 2)
		return SLASH_EUSAGE;

	enable = (uint8_t)strtoul(slash->argv[1], &endptr, 10);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	if (slash->argc > 2) {
		boots = (uint8_t)strtoul(slash->argv[2], &endptr, 10);
		if (*endptr != '\0')
			return SLASH_EUSAGE;
	}

	printf("%u\n", boots);

	ret = ais_eth_set(ais_node, ais_timeout, enable, boots);
	if (ret < 0) {
		printf("error: failed to set eth config: %d\n", ret);
		return -1;
	}

	return 0;
}
slash_command_subsub(ais, eth, set, cmd_ais_eth_set, "<enable> [boots]",
		     "Enable/disable ethernet");

static int cmd_ais_eth_get(struct slash *slash)
{
	int ret;
	uint8_t enable;
	uint16_t boots;

	ret = ais_eth_get(ais_node, ais_timeout, &enable, &boots);
	if (ret < 0) {
		printf("error: failed to get eth config: %d\n", ret);
		return -1;
	}

	printf("enable: %u\n", enable);
	printf("boots:  %u\n", boots);

	return 0;
}
slash_command_subsub(ais, eth, get, cmd_ais_eth_get, NULL,
		     "Get ethernet configuration");
