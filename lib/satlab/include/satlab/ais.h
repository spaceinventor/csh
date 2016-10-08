/*
 * Copyright (c) 2015 Satlab Aps <satlab@satlab.org>
 * All rights reserved.
 *
 * AIS receiver interface
 */

#ifndef _SL_AIS_H_
#define _SL_AIS_H_

#include <stdint.h>
#include <string.h>

/* AIS frame - total size is 40 bytes */
#define AIS_FRAME_SIZE			26
#define AIS_FRAME_STATIC_SIZE		55
#define AIS_STORE_DEFAULT_FLAGS		0x00

#define AIS_FRAME_FLAG_CRC_FAIL		0x00
#define AIS_FRAME_FLAG_CRC_OK		0x01

#define AIS_FRAME_FLAG_CH1		0x00
#define AIS_FRAME_FLAG_CH2		0x02
#define AIS_FRAME_FLAG_CH3		0x04
#define AIS_FRAME_FLAG_CH4		0x06

typedef struct ais_frame_t {
	/* Message sequence number - 0xffffffff for free */
	uint32_t seq_nr;

	/* UNIX Timestamp of reception */
	uint32_t time;

	/* RSSI is the 8 LSB */
	uint8_t rssi;

	/* Optional value X, Y and Z */
	uint8_t x;
	uint8_t y;
	uint8_t z;

	/* Flag bits */
	uint8_t flags;

	/* Data size in in bytes */
	uint8_t size;

	/* The RAW AIS MSG. NRZI decoded, HDLC decoded-incl CRC
	 * but without start and stop flags (0x7E)
	 */
	uint8_t data[AIS_FRAME_SIZE];
} __attribute__((packed)) ais_frame_t;

typedef struct ais_frame_static_t {
	/* Message sequence number - 0xffffffff for free */
	uint32_t seq_nr;

	/* UNIX Timestamp of reception */
	uint32_t time;

	/* RSSI is the 8 LSB */
	uint8_t rssi;

	/* Optional value X, Y and Z */
	uint8_t x;
	uint8_t y;
	uint8_t z;

	/* Flag bits */
	uint8_t flags;

	/* Data size in in bytes */
	uint8_t size;

	/* The RAW AIS MSG. NRZI decoded, HDLC decoded-incl CRC
	 * but without start and stop flags (0x7E)
	 */
	uint8_t data[AIS_FRAME_STATIC_SIZE];
} __attribute__((packed)) ais_frame_static_t;

/* Short AIS frame for position reports */
typedef struct {
	uint32_t mmsi;
	uint32_t time;
	uint8_t rssi;
	uint8_t msgid;
	uint8_t flags;
	int32_t lat;
	int32_t lon;
} __attribute__((packed)) ais_short_frame_t;

/* AIS receiver CSP ports */
#define AIS_PORT_STATUS			8
#define AIS_PORT_MODE	 		9
#define AIS_PORT_BEACON			10 /* Not used */
#define AIS_PORT_AIS_STORE		11
#define AIS_PORT_AIS_STORE_SHORT	12
#define AIS_PORT_BTP  			13
#define AIS_PORT_ADMIN			14
#define AIS_PORT_AISBOOT		15 /* Used by aisloader */
#define AIS_PORT_MISC			16
#define AIS_PORT_AIS_RX_STATUS		17
#define AIS_PORT_VERSION 		18
#define AIS_PORT_CONFIG			19
#define AIS_PORT_AIS_STORE_STATIC	20
/* Added in version 5.30 */
#define AIS_PORT_STORE_CONFIG		21
#define AIS_PORT_STORE_REQUEST		22
#define AIS_PORT_TIME_CONFIG		23
/* Added in version 6.00 */
#define AIS_PORT_CHANNEL_CONFIG		24
/* Added in version 6.20 */
#define AIS_PORT_ETH_CONFIG		25
/* Added in version 5.60 */
#define AIS_PORT_DEMOD			30
#define AIS_PORT_DEMOD_USER		31

/* AIS receive status */
typedef struct ais_rx_status_t {
	/* Current filter message IDs */
	uint32_t id;
	/* Current filter minimum latitude */
	int32_t latmin;
	/* Current filter maximum latitude */
	int32_t latmax;
	/* Current filter minimum longitude */
	int32_t lonmin;
	/* Current filter maximum longitude */
	int32_t lonmax;
	/* Number of correctly decoded messages */
	uint32_t crc_ok;
	/* Number of detected messages with incorrent CRC */
	uint32_t crc_error;
	/* Number of decoded messages that passed the filter */
	uint32_t filter_match_ok;
	/* Number of messages rejected on message ID */
	uint32_t rej_id;
	/* Number of messages rejected on position */
	uint32_t rej_pos;
	/* Number of messages rejected on MMSI */
	uint32_t rej_mmsi;
	/* Number of messages rejected on country code */
	uint32_t rej_cc;
	/* Number of messages rejected on timestamp */
	uint32_t rej_time;
} __attribute__((packed)) ais_rx_status_t;


/*
 *  AIS_PORT_STATUS
 */
#define AIS_RUNNING_FROM_SD		0x00
#define AIS_RUNNING_FROM_NOR		0x01
#define AIS_RUNNING_FROM_FALLBACK	0x02
typedef struct {
	/* Number of critical errors in this session */
	uint8_t  critical_errors;
	/* Number of warnings in this session */
	uint8_t  warnings;
	/* Number of samples processed in this session */
	uint16_t runs;
	/* Number of packets deteceted */
	uint16_t packs_detected;
	/* 0=SD, 1=NOR, 2=fallback */
	uint8_t  running_from;
	/* Boot count */
	uint16_t bootcount;
	/* Packets with valid CRC16 */
	uint32_t crc_ok;
	/* Unique MMSIs received */
	uint16_t unique_mmsi;
	/* Newest MMSI */
	uint32_t latest_mmsi;
	/* Longitude of newest message */
	int32_t  latest_long;
	/* Latitude of newest message */
	int32_t  latest_lat;
	/* RSSI of 100 kHz channel bandwidth */
	uint8_t  rssi;
	/* Status flags */
	uint8_t  flags;
} __attribute__((packed)) ais2_port_status_t;


/*
 * AIS_PORT_MODE
 */
#define AIS2_MODE_IDLE				0
#define AIS2_MODE_RUNNING_SINGLE		1
#define AIS2_MODE_RUNNING_CONT			2
#define AIS2_MODE_RUNNING_AND_SAVING_SINGLE	3
#define AIS2_MODE_RUNNING_AND_SAVING_CONT	4

#define AIS_STORAGE_ID_SDCARD			0
#define AIS_STORAGE_ID_NORFLASH			1
typedef struct ais2_port_mode_t {
	/* New receiver mode. Must be one of AIS2_MODE_IDLE,
	 * AIS2_MODE_RUNNING_SINGLE, AIS2_MODE_RUNNING_AND_SAVING_SINGLE
	 * or AIS2_MODE_RUNNING_AND_SAVING_CONT */
	uint8_t command_code;
	/* Where to store samples, 0=sd, 1=nor */
	uint8_t storage_id;
	/* Number of first file */
	uint8_t file_name_no;
	/* Number of samples to process */
	uint8_t num_of_samples;
} __attribute__((packed)) ais2_port_mode_t;


/*
 * AIS_PORT_AIS_STORE/AIS_PORT_AIS_STORE_SHORT
 */
#define	AIS_BACKEND_FILE		0
#define AIS_BACKEND_UNIQUE		1
#define AIS_BACKEND_STATIC		2
typedef struct {
	/* Request packets from sequence number. Use 0 to request newest */
	uint32_t from_seq_nr;
	/* Maximum number of frames to reply */
	uint16_t max_num_of_frames;
	/* Storage backend to request from. Must be AIS_BACKEND_FILE or AIS_BACKEND_UNIQUE */
	uint8_t backend;
	/* Number of ais_frame_t to include per reply (1-4) */
	uint8_t num_of_frames_per_reply;
	/* Delay in 10 milliseconds between each packet */
	uint8_t delay_between_replys;
} __attribute__((packed)) ais_port_ais_store_req_t;

/* Message store reply */
#define MAX_STORE_REPLY_FRAMES		4
typedef struct {
	/* Current sequence number */
	uint32_t seq_nr;
	ais_frame_t ais_frame[MAX_STORE_REPLY_FRAMES];
} __attribute__((packed)) ais_port_ais_store_rep_t;

#define MAX_STORE_SHORT_REPLY_FRAMES	10
typedef struct {
	/* Current sequence number */
	uint32_t seq_nr;
	ais_short_frame_t frame[MAX_STORE_SHORT_REPLY_FRAMES];
} __attribute__((packed)) ais_port_ais_short_rep_t;

#define MAX_STORE_STATIC_REPLY_FRAMES	4
typedef struct {
	/* Current sequence number */
	uint32_t seq_nr;
	ais_frame_static_t frame[MAX_STORE_STATIC_REPLY_FRAMES];
} __attribute__((packed)) ais_port_ais_static_rep_t;


/*
 * AIS_PORT_MISC
 */
#define AIS_PORT_MISC_COMMAND_TYPE_UPDATE_UNIQUE 0
typedef struct {
	uint8_t ctype;
	uint32_t num;
} __attribute__((packed)) ais2_port_misc_t;


/*
 * AIS_PORT_ADMIN
 */
#define AIS_PORT_ADMIN_RESET_SD_CARD	5
#define AIS_PORT_ADMIN_RESET_NOR_Flash	6
#define AIS_PORT_ADMIN_CUSTOM_COMMAND	22

typedef struct {
	int8_t command_type;
	char command[80];
} __attribute__((packed)) ais2_port_admin_t;


/*
 * AIS_PORT_CONFIG
 */
typedef struct ais2_port_config_t {
	/* Configuration is valid for this many boots
	 * (0 = do not store, 0xffff = infinite boots)*/
	uint16_t boots;
	/* ADC PGA gain step */
	uint8_t adc_gain;
	/* ADF filter gain step */
	uint8_t filter_gain;
	/* ADF LNA gain step */
	uint8_t lna_gain;
	/* Autostart */
	uint8_t autostart;
} __attribute__((packed)) ais2_port_config_t;

/* Message callbacks */
typedef int (*messagecb)(ais_frame_t *frame);
typedef int (*shortmessagecb)(uint32_t seq_nr, ais_short_frame_t *frame);
typedef int (*staticmessagecb)(ais_frame_static_t *frame);

/*
 * AIS_PORT_STORE_CONFIG
 */

/* Types */
#define AIS_PORT_STORE_CONFIG_SET_SIZE		1
#define AIS_PORT_STORE_CONFIG_GET_STATUS	2

/* Error codes */
#define AIS_PORT_STORE_CONFIG_ERROR_OK		0 /* No error */
#define AIS_PORT_STORE_CONFIG_ERROR_INVAL	1 /* Invalid value */
#define AIS_PORT_STORE_CONFIG_ERROR_NOENT	2 /* No such entry */
#define AIS_PORT_STORE_CONFIG_ERROR_IOERR	3 /* I/O error */

/* Set size */
typedef struct ais_port_store_config_size_req {
	/* Request type, must be AIS_PORT_STORE_CONFIG_SET_SIZE */
	uint8_t type;
	/* The backend to configure */
	uint8_t backend;
	/* New size in messages */
	uint32_t size;
} __attribute__((packed)) ais_port_store_config_size_req_t;

typedef struct ais_port_store_config_size_rep {
	/* Reply type, must be AIS_PORT_STORE_CONFIG_SET_SIZE */
	uint8_t type;
	/* Error code */
	uint8_t error;
	/* The backend to configure */
	uint8_t backend;
	/* New size in messages */
	uint32_t size;
} __attribute__((packed)) ais_port_store_config_size_rep_t;

/* Get status */
typedef struct ais_port_store_config_status_req {
	/* Request type, must be AIS_PORT_STORE_CONFIG_GET_STATUS */
	uint8_t type;
	/* The backend to configure */
	uint8_t backend;
} __attribute__((packed)) ais_port_store_config_status_req_t;

typedef struct ais_port_store_config_status_rep {
	/* Reply type, must be AIS_PORT_STORE_CONFIG_GET_STATUS */
	uint8_t type;
	/* Error code */
	uint8_t error;
	/* The backend to configure */
	uint8_t backend;
	/* Current size */
	uint32_t size;
	/* Current sequence number */
	uint32_t seq;
	/* Current count number */
	uint32_t count;
} __attribute__((packed)) ais_port_store_config_status_rep_t;

/*
 * AIS_PORT_STORE_REQUEST
 */

/* Types */
#define AIS_PORT_STORE_REQUEST_BITMAP		1

/* Bitmap size */
#define AIS_PORT_STORE_REQUEST_BITMAP_SIZE	32 /* bytes */

/* Request flags */
#define AIS_PORT_STORE_REQUEST_FLAG_SHORT	(1 << 0)

typedef struct ais_port_store_request_bitmap_req {
	/* Request type, must be AIS_PORT_STORE_REQUEST_BITMAP */
	uint8_t type;
	/* Storage backend to request from */
	uint8_t backend;
	/* Request flags */
	uint8_t flags;
	/* Number of frames to include per reply */
	uint8_t frames_per_reply;
	/* Delay in 10 milliseconds between each packet */
	uint8_t delay;
	/* Request packets from sequence number. Use 0 to request newest */
	uint32_t start;
	/* Bitmap of frames, bit 0 = start, bit n = start - n */
	uint8_t bitmap[AIS_PORT_STORE_REQUEST_BITMAP_SIZE];
} __attribute__((packed)) ais_port_store_request_bitmap_req_t;

/* Replies will be ais_port_ais_{store,short,static}_rep_t */

/*
 * AIS_PORT_TIME_CONFIG
 */

/* Types */
#define AIS_PORT_TIME_CONFIG_SET_TIME		1
#define AIS_PORT_TIME_CONFIG_GET_TIME		2

/* Error codes */
#define AIS_PORT_TIME_CONFIG_ERROR_OK		0 /* No error */
#define AIS_PORT_TIME_CONFIG_ERROR_INVAL	1 /* Invalid value */

typedef struct ais_port_time_config_set_time_req {
	/* Request type, must be AIS_PORT_TIME_CONFIG_SET_TIME */
	uint8_t type;
	/* New seconds */
	uint32_t sec;
	/* New nanoseconds */
	uint32_t nsec;
} __attribute__((packed)) ais_port_time_config_set_time_req_t;

typedef struct ais_port_time_config_set_time_rep {
	/* Reply type, must be AIS_PORT_TIME_CONFIG_SET_TIME */
	uint8_t type;
	/* Error code */
	uint8_t error;
	/* New seconds */
	uint32_t sec;
	/* New nanoseconds */
	uint32_t nsec;
} __attribute__((packed)) ais_port_time_config_set_time_rep_t;

typedef struct ais_port_time_config_get_time_req {
	/* Request type, must be AIS_PORT_TIME_CONFIG_GET_TIME */
	uint8_t type;
} __attribute__((packed)) ais_port_time_config_get_time_req_t;

typedef struct ais_port_time_config_get_time_rep {
	/* Reply type, must be AIS_PORT_TIME_CONFIG_GET_TIME */
	uint8_t type;
	/* Error code */
	uint8_t error;
	/* New seconds */
	uint32_t sec;
	/* New nanoseconds */
	uint32_t nsec;
} __attribute__((packed)) ais_port_time_config_get_time_rep_t;

/*
 * AIS_PORT_DEMOD
 */

#define AIS_PORT_DEMOD_SET		1
#define AIS_PORT_DEMOD_GET		2

typedef struct ais_port_demod_config {
	/* Reply type, must be AIS_PORT_DEMOD_SET/GET */
	uint8_t type;
	/* Error code */
	uint8_t error;
	/* Configuration is valid for this many boots
	 * (0 = do not store, 0xffff = infinite boots)*/
	uint16_t boots;
	/* Absolute path to demodulator plugin */
	char path[64];
} __attribute__((packed)) ais_port_demod_config_t;

/*
 * AIS_PORT_CHANNEL_CONFIG
 */

#define AIS_PORT_CHANNEL_SET		1
#define AIS_PORT_CHANNEL_GET		2

#define AIS_PORT_CHANNEL_CH1_2		1
#define AIS_PORT_CHANNEL_CH3_4		2

typedef struct ais_port_channel_config_s {
	/* Reply type, must be AIS_PORT_CHANNEL_SET/GET */
	uint8_t type;
	/* Error code */
	uint8_t error;
	/* Channel selection */
	uint8_t channels;
	/* Flags */
	uint32_t flags;
} __attribute__((packed)) ais_port_channel_config_t;

/*
 * AIS_PORT_ETH_CONFIG
 */

#define AIS_PORT_ETH_SET		1
#define AIS_PORT_ETH_GET		2

typedef struct ais_port_eth_config_s {
	/* Reply type, must be AIS_PORT_ETH_SET/GET */
	uint8_t type;
	/* Error code */
	uint8_t error;
	/* Channel selection */
	uint8_t enable;
	/* Configuration is valid for this many boots
	 * (0 = do not store, 0xffff = infinite boots)*/
	uint16_t boots;
	/* Flags */
	uint32_t flags;
} __attribute__((packed)) ais_port_eth_config_t;

/*
 * AIS_PORT_STATUS
 */
int ais_get_status(int node, uint32_t timeout, ais2_port_status_t *status);

/*
 * AIS_PORT_AIS_RX_STATUS
 */
int ais_get_rx_status(int node, uint32_t timeout, ais_rx_status_t *rx_status);

/*
 * AIS_PORT_MODE
 */
int ais_set_mode(int node, uint32_t timeout,
		 uint8_t mode, uint8_t storage,
		 uint8_t fileno, uint8_t count_samples);

int ais_get_mode(int node, uint32_t timeout,
		 uint8_t *mode, uint8_t *storage,
		 uint8_t *fileno, uint8_t *count_samples);

/*
 * AIS_PORT_CONFIG
 */
int ais_set_config(int node, uint32_t timeout,
		   uint16_t boots, uint8_t adc_gain,
		   uint8_t filter_gain, uint8_t lna_gain, uint8_t autostart);

int ais_get_config(int node, uint32_t timeout,
		   uint16_t *boots, uint8_t *adc_gain, uint8_t *filter_gain,
		   uint8_t *lna_gain, uint8_t *autostart);

/*
 * AIS_PORT_VERSION
 */
int ais_get_version(int node, uint32_t timeout, char *version);

/*
 * AIS_PORT_AIS_STORE
 */
int ais_get_frames(int node, uint32_t timeout, uint8_t backend, uint32_t from,
		   uint16_t count, messagecb callback);

/*
 * AIS_PORT_AIS_STORE_SHORT
 */
int ais_get_short_frames(int node, uint32_t timeout, unsigned int backend,
		         unsigned int from, unsigned count, shortmessagecb callback);

/*
 * AIS_PORT_AIS_STORE_STATIC
 */
int ais_get_static_frames(int node, uint32_t timeout, unsigned int backend,
		         unsigned int from, unsigned count, staticmessagecb callback);

/*
 * AIS_PORT_STORE_CONFIG
 */
int ais_get_store_status(int node, uint32_t timeout, unsigned int backend,
			 uint32_t *size, uint32_t *count, uint32_t *seq);

int ais_set_store_size(int node, uint32_t timeout, unsigned int backend,
		       uint32_t size);

/*
 * AIS_PORT_STORE_REQUEST
 */
int ais_request_bitmap_frames(int node, uint32_t timeout, uint8_t backend,
			      uint32_t start, uint8_t frames_per_reply,
			      uint8_t bitmap[AIS_PORT_STORE_REQUEST_BITMAP_SIZE],
			      messagecb callback);

int ais_request_bitmap_static(int node, uint32_t timeout, uint8_t backend,
			      uint32_t start, uint8_t frames_per_reply,
			      uint8_t bitmap[AIS_PORT_STORE_REQUEST_BITMAP_SIZE],
			      staticmessagecb callback);

int ais_request_bitmap_short(int node, uint32_t timeout, uint8_t backend,
			     uint32_t start, uint8_t frames_per_reply,
			     uint8_t bitmap[AIS_PORT_STORE_REQUEST_BITMAP_SIZE],
			     shortmessagecb callback);

/*
 * AIS_PORT_DEMOD
 */
int ais_demod_set(int node, uint32_t timeout, char *path, uint16_t boots);

int ais_demod_get(int node, uint32_t timeout, char *path, uint16_t *boots);

/*
 * AIS_PORT_DEMOD_USER
 */
int ais_demod_user(int node, uint32_t timeout,
		   uint8_t *tx, size_t txlen,
		   uint8_t *rx, size_t rxlen);

/*
 * AIS_PORT_TIME_CONFIG
 */
int ais_set_time(int node, uint32_t timeout, uint32_t sec, uint32_t nsec);

int ais_get_time(int node, uint32_t timeout, uint32_t *sec, uint32_t *nsec); 

/*
 * AIS_PORT_CHANNEL_CONFIG
 */
int ais_channels_set(int node, uint32_t timeout, uint8_t channels, uint32_t flags);

int ais_channels_get(int node, uint32_t timeout, uint8_t *channels);

/*
 * AIS_PORT_ETH_CONFIG
 */
int ais_eth_set(int node, uint32_t timeout, uint8_t enable, uint16_t boots);

int ais_eth_get(int node, uint32_t timeout, uint8_t *enable, uint16_t *boots);

#endif /* _SL_AIS_H_ */
