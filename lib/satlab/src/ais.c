/*
 * Copyright (c) 2015 Satlab Aps <satlab@satlab.com>
 * All rights reserved.
 *
 * AIS receiver helper functions
 */

#include <stdio.h>
#include <errno.h>
#include <satlab/ais.h>

#include <csp/csp.h>
#include <csp/csp_endian.h>

/*
 * AIS_PORT_STATUS
 */
int ais_get_status(int node, uint32_t timeout, ais2_port_status_t *status)
{
	uint8_t dummy = 0;

	if (!status)
		return -EINVAL;

	if (csp_transaction(CSP_PRIO_NORM, node, AIS_PORT_STATUS, timeout,
			    &dummy, sizeof(dummy), status, sizeof(*status)) != sizeof(*status))
		return -EINVAL;

	/* Convert to host byte order */
	status->runs 		= csp_ntoh16(status->runs);
	status->packs_detected 	= csp_ntoh16(status->packs_detected);
	status->bootcount 	= csp_ntoh16(status->bootcount);
	status->crc_ok 		= csp_ntoh32(status->crc_ok);
	status->unique_mmsi	= csp_ntoh16(status->unique_mmsi);
	status->latest_mmsi 	= csp_ntoh32(status->latest_mmsi);
	status->latest_long 	= csp_ntoh32(status->latest_long);
	status->latest_lat 	= csp_ntoh32(status->latest_lat);

	return 0;
}

/*
 * AIS_PORT_AIS_RX_STATUS
 */
int ais_get_rx_status(int node, uint32_t timeout, ais_rx_status_t *rx_status)
{
	uint8_t dummy = 0;

	if (!rx_status)
		return -EINVAL;

	if (csp_transaction(CSP_PRIO_NORM, node, AIS_PORT_AIS_RX_STATUS,
			    timeout, &dummy, sizeof(dummy), rx_status,
			    sizeof(*rx_status)) != sizeof(*rx_status))
		return -EINVAL;

	/* Convert to host byte order */
	rx_status->id 		= csp_ntoh32(rx_status->id);
	rx_status->latmin 	= csp_ntoh32(rx_status->latmin);
	rx_status->latmax 	= csp_ntoh32(rx_status->latmax);
	rx_status->lonmin 	= csp_ntoh32(rx_status->lonmin);
	rx_status->lonmax 	= csp_ntoh32(rx_status->lonmax);
	rx_status->crc_ok 	= csp_ntoh32(rx_status->crc_ok);
	rx_status->crc_error 	= csp_ntoh32(rx_status->crc_error);
	rx_status->filter_match_ok = csp_ntoh32(rx_status->filter_match_ok);
	rx_status->rej_id 	= csp_ntoh32(rx_status->rej_id);
	rx_status->rej_pos 	= csp_ntoh32(rx_status->rej_pos);
	rx_status->rej_mmsi	= csp_ntoh32(rx_status->rej_mmsi);
	rx_status->rej_cc 	= csp_ntoh32(rx_status->rej_cc);
	rx_status->rej_time 	= csp_ntoh32(rx_status->rej_time);

	return 0;
}

/*
 * AIS_PORT_MODE
 */
int ais_set_mode(int node, uint32_t timeout,
		 uint8_t mode, uint8_t storage,
		 uint8_t fileno, uint8_t count_samples)
{
	ais2_port_mode_t req;
	ais2_port_mode_t rep;

	req.command_code = mode;
	req.storage_id = storage;
	req.file_name_no = fileno;
	req.num_of_samples = count_samples;

	if (csp_transaction(CSP_PRIO_NORM, node, AIS_PORT_MODE, timeout, &req,
			    sizeof(req), &rep, sizeof(rep)) != sizeof(rep))
		return -EINVAL;

	return 0;
}

int ais_get_mode(int node, uint32_t timeout,
		 uint8_t *mode, uint8_t *storage,
		 uint8_t *fileno, uint8_t *count_samples)
{
	/* Single byte to request mode */
	uint8_t req = 0;
	ais2_port_mode_t rep;

	if (csp_transaction(CSP_PRIO_NORM, node, AIS_PORT_MODE, timeout, &req,
			    sizeof(req), &rep, sizeof(rep)) != sizeof(rep))
		return -EINVAL;

	if (mode)
		*mode = rep.command_code;
	if (storage)
		*storage = rep.storage_id;
	if (fileno)
		*fileno = rep.file_name_no;
	if (count_samples)
		*count_samples = rep.num_of_samples;

	return 0;
}

/*
 * AIS_PORT_CONFIG
 */
int ais_set_config(int node, uint32_t timeout,
		   uint16_t boots, uint8_t adc_gain, uint8_t filter_gain,
		   uint8_t lna_gain, uint8_t autostart)
{
	ais2_port_config_t req;
	ais2_port_config_t rep;

	req.boots = csp_hton16(boots);
	req.adc_gain = adc_gain;
	req.filter_gain = filter_gain;
	req.lna_gain = lna_gain;
	req.autostart = autostart;

	if (csp_transaction(CSP_PRIO_NORM, node, AIS_PORT_CONFIG, timeout, &req,
			    sizeof(req), &rep, sizeof(rep)) != sizeof(rep))
		return -EINVAL;

	return 0;
}

int ais_get_config(int node, uint32_t timeout,
		   uint16_t *boots, uint8_t *adc_gain, uint8_t *filter_gain,
		   uint8_t *lna_gain, uint8_t *autostart)
{
	/* Single byte to request config */
	uint8_t req;
	ais2_port_config_t rep;

	if (csp_transaction(CSP_PRIO_NORM, node, AIS_PORT_CONFIG, timeout, &req,
			    sizeof(req), &rep, sizeof(rep)) != sizeof(rep))
		return -EINVAL;

	if (boots)
		*boots = csp_ntoh16(rep.boots);
	if (adc_gain)
		*adc_gain = rep.adc_gain;
	if (filter_gain)
		*filter_gain = rep.filter_gain;
	if (lna_gain)
		*lna_gain = rep.lna_gain;
	if (autostart)
		*autostart = rep.autostart;

	return 0;
}

/*
 * AIS_PORT_AIS_STORE
 */
int ais_get_frames(int node, uint32_t timeout, uint8_t backend, uint32_t from,
		   uint16_t count, messagecb callback)
{
	int i, j, frames;
	ais_port_ais_store_req_t *req;
	ais_port_ais_store_rep_t *rep;
	csp_packet_t *packet;
	csp_conn_t *conn;

	packet = csp_buffer_get(sizeof(*req));
	if (!packet)
		return -ENOMEM;

	req = (ais_port_ais_store_req_t *) packet->data;
	req->from_seq_nr = csp_hton32(from);
	req->max_num_of_frames = csp_hton16(count);
	req->backend = backend;
	req->num_of_frames_per_reply = 2;
	req->delay_between_replys = 0;
	packet->length = sizeof(*req);

	frames = (count + req->num_of_frames_per_reply - 1) / req->num_of_frames_per_reply;

	conn = csp_connect(CSP_PRIO_NORM, node, AIS_PORT_AIS_STORE, timeout, CSP_O_NONE);
	if (!conn) {
		csp_buffer_free(packet);
		return -ECONNREFUSED;
	}

	if (!csp_send(conn, packet, timeout)) {
		csp_close(conn);
		csp_buffer_free(packet);
		return -ETIMEDOUT;
	}

	for (i = 0; i < frames; i++) {
		packet = csp_read(conn, timeout);
		if (!packet) {
			csp_close(conn);
			return -ETIMEDOUT;
		}

		rep = (ais_port_ais_store_rep_t *) packet->data;

		rep->seq_nr = csp_ntoh32(rep->seq_nr);

		for (j = 0; j < 2; j++) {
			if (rep->ais_frame[j].seq_nr != 0xffffffff) {
				rep->ais_frame[j].seq_nr = csp_ntoh32(rep->ais_frame[j].seq_nr);
				rep->ais_frame[j].time = csp_ntoh32(rep->ais_frame[j].time);
				callback(&rep->ais_frame[j]);
			}
		}

		csp_buffer_free(packet);
	}

	csp_close(conn);

	return 0;
}

/*
 * AIS_PORT_AIS_STORE_SHORT
 */
int ais_get_short_frames(int node, uint32_t timeout, unsigned int backend,
		         unsigned int from, unsigned count, shortmessagecb callback)
{
	int i, j, frames;
	uint32_t seq_nr;
	ais_port_ais_store_req_t *req;
	ais_port_ais_short_rep_t *rep;
	csp_packet_t *packet;
	csp_conn_t *conn;

	packet = csp_buffer_get(sizeof(*req));
	if (!packet)
		return -ENOMEM;

	req = (ais_port_ais_store_req_t *) packet->data;
	req->from_seq_nr = csp_hton32(from);
	req->max_num_of_frames = csp_hton16(count);
	req->backend = backend;
	req->num_of_frames_per_reply = 4;
	req->delay_between_replys = 0;
	packet->length = sizeof(*req);

	frames = (count + req->num_of_frames_per_reply - 1) / req->num_of_frames_per_reply;

	conn = csp_connect(CSP_PRIO_NORM, node, AIS_PORT_AIS_STORE_SHORT, timeout, CSP_O_NONE);
	if (!conn) {
		csp_buffer_free(packet);
		return -ECONNREFUSED;
	}

	if (!csp_send(conn, packet, timeout)) {
		csp_close(conn);
		csp_buffer_free(packet);
		return -ETIMEDOUT;
	}

	for (i = 0; i < frames; i++) {
		packet = csp_read(conn, timeout);
		if (!packet) {
			csp_close(conn);
			return -ETIMEDOUT;
		}

		rep = (ais_port_ais_short_rep_t *) packet->data;

		rep->seq_nr = csp_ntoh32(rep->seq_nr);
		seq_nr = rep->seq_nr;

		for (j = 0; j < 4; j++) {
			if (rep->frame[j].mmsi != 0xffffffff) {
				rep->frame[j].mmsi = csp_ntoh32(rep->frame[j].mmsi);
				rep->frame[j].time = csp_ntoh32(rep->frame[j].time);
				rep->frame[j].lat = csp_ntoh32(rep->frame[j].lat);
				rep->frame[j].lon = csp_ntoh32(rep->frame[j].lon);
				callback(seq_nr - j, &rep->frame[j]);
			}
		}

		csp_buffer_free(packet);
	}

	csp_close(conn);

	return 0;
}


/*
 * AIS_PORT_AIS_STORE_STATIC
 */
int ais_get_static_frames(int node, uint32_t timeout, unsigned int backend,
		         unsigned int from, unsigned count, staticmessagecb callback)
{
	int i, j, frames;
	ais_port_ais_store_req_t *req;
	ais_port_ais_static_rep_t *rep;
	csp_packet_t *packet;
	csp_conn_t *conn;

	if (backend != AIS_BACKEND_STATIC)
		return -EINVAL;

	packet = csp_buffer_get(sizeof(*req));
	if (!packet)
		return -ENOMEM;

	req = (ais_port_ais_store_req_t *) packet->data;
	req->from_seq_nr = csp_hton32(from);
	req->max_num_of_frames = csp_hton16(count);
	req->backend = backend;
	req->num_of_frames_per_reply = 1;
	req->delay_between_replys = 0;
	packet->length = sizeof(*req);

	frames = (count + req->num_of_frames_per_reply - 1) / req->num_of_frames_per_reply;

	conn = csp_connect(CSP_PRIO_NORM, node, AIS_PORT_AIS_STORE_STATIC, timeout, CSP_O_NONE);
	if (!conn) {
		csp_buffer_free(packet);
		return -ECONNREFUSED;
	}

	if (!csp_send(conn, packet, timeout)) {
		csp_close(conn);
		csp_buffer_free(packet);
		return -ETIMEDOUT;
	}

	for (i = 0; i < frames; i++) {
		packet = csp_read(conn, timeout);
		if (!packet) {
			csp_close(conn);
			return -ETIMEDOUT;
		}

		rep = (ais_port_ais_static_rep_t *) packet->data;

		rep->seq_nr = csp_ntoh32(rep->seq_nr);

		for (j = 0; j < 1; j++) {
			if (rep->frame[j].seq_nr != 0xffffffff) {
				rep->frame[j].seq_nr = csp_ntoh32(rep->frame[j].seq_nr);
				rep->frame[j].time = csp_ntoh32(rep->frame[j].time);
				callback(&rep->frame[j]);
			}
		}

		csp_buffer_free(packet);
	}

	csp_close(conn);

	return 0;
}

/*
 * AIS_PORT_STORE_CONFIG
 */
int ais_get_store_status(int node, uint32_t timeout, unsigned int backend,
			 uint32_t *size, uint32_t *count, uint32_t *seq)
{
	ais_port_store_config_status_req_t req;
	ais_port_store_config_status_rep_t rep;

	req.type = AIS_PORT_STORE_CONFIG_GET_STATUS;
	req.backend = backend;

	if (csp_transaction(CSP_PRIO_NORM, node, AIS_PORT_STORE_CONFIG, timeout,
			    &req, sizeof(req), &rep, sizeof(rep)) != sizeof(rep))
		return -EINVAL;

	if (rep.error != 0) {
		*size = 0;
		*count = 0;
		*seq = 0;
		return -EINVAL;
	}

	*size = csp_ntoh32(rep.size);
	*count = csp_ntoh32(rep.count);
	*seq = csp_ntoh32(rep.seq);

	return 0;
}

int ais_set_store_size(int node, uint32_t timeout, unsigned int backend,
		       uint32_t size)
{
	ais_port_store_config_size_req_t req;
	ais_port_store_config_size_rep_t rep;

	req.type = AIS_PORT_STORE_CONFIG_SET_SIZE;
	req.backend = backend;
	req.size = csp_hton32(size);

	if (csp_transaction(CSP_PRIO_NORM, node, AIS_PORT_STORE_CONFIG, timeout,
			    &req, sizeof(req), &rep, sizeof(rep)) != sizeof(rep))
		return -EINVAL;

	if (rep.error != 0 || csp_ntoh32(rep.size) != size)
		return -EINVAL;

	return 0;
}

static int array_weight(uint8_t *array, int length)
{
	int i, c;
	uint8_t *cur = array, v;
	int weight = 0;

	for (i = 0; i < length; i++) {
		v = cur[i];
		for (c = 0; v; c++) {
			/* clear the least significant bit set */
			v &= v - 1;
		}
		weight += c;
	}

	return weight;
}

/*
 * AIS_PORT_STORE_REQUEST
 */
int ais_request_bitmap_frames(int node, uint32_t timeout, uint8_t backend,
			      uint32_t start, uint8_t frames_per_reply,
			      uint8_t bitmap[AIS_PORT_STORE_REQUEST_BITMAP_SIZE],
			      messagecb callback)
{
	int i, j, count, frames;
	ais_port_store_request_bitmap_req_t *req;
	ais_port_ais_store_rep_t *rep;
	csp_packet_t *packet;
	csp_conn_t *conn;

	packet = csp_buffer_get(sizeof(*req));
	if (!packet)
		return -ENOMEM;

	req = (ais_port_store_request_bitmap_req_t *) packet->data;
	req->type = AIS_PORT_STORE_REQUEST_BITMAP;
	req->backend = backend;
	req->flags = 0;

	req->start = csp_hton32(start);
	req->frames_per_reply = frames_per_reply;
	req->delay = 0;
	memcpy(req->bitmap, bitmap, sizeof(req->bitmap));

	packet->length = sizeof(*req);

	/* Calculated expected reply frames */
	count = array_weight(bitmap, sizeof(req->bitmap));
	frames = (count + frames_per_reply - 1) / frames_per_reply;

	conn = csp_connect(CSP_PRIO_NORM, node, AIS_PORT_STORE_REQUEST, timeout, CSP_O_NONE);
	if (!conn) {
		csp_buffer_free(packet);
		return -ECONNREFUSED;
	}

	if (!csp_send(conn, packet, timeout)) {
		csp_buffer_free(packet);
		csp_close(conn);
		return -EIO;
	}

	for (i = 0; i < frames; i++) {
		packet = csp_read(conn, timeout);
		if (!packet) {
			csp_close(conn);
			return -ETIMEDOUT;
		}

		rep = (ais_port_ais_store_rep_t *) packet->data;

		rep->seq_nr = csp_ntoh32(rep->seq_nr);

		for (j = 0; j < frames_per_reply; j++) {
			if (rep->ais_frame[j].seq_nr != 0xffffffff) {
				rep->ais_frame[j].seq_nr = csp_ntoh32(rep->ais_frame[j].seq_nr);
				rep->ais_frame[j].time = csp_ntoh32(rep->ais_frame[j].time);
				callback(&rep->ais_frame[j]);
			}
		}

		csp_buffer_free(packet);
	}

	csp_close(conn);

	return 0;
}

int ais_request_bitmap_short(int node, uint32_t timeout, uint8_t backend,
			     uint32_t start, uint8_t frames_per_reply,
			     uint8_t bitmap[AIS_PORT_STORE_REQUEST_BITMAP_SIZE],
			     shortmessagecb callback)
{
	int i, j, count, frames;
	uint32_t seq_nr;
	ais_port_store_request_bitmap_req_t *req;
	ais_port_ais_short_rep_t *rep;
	csp_packet_t *packet;
	csp_conn_t *conn;

	packet = csp_buffer_get(sizeof(*req));
	if (!packet)
		return -ENOMEM;

	req = (ais_port_store_request_bitmap_req_t *) packet->data;
	req->type = AIS_PORT_STORE_REQUEST_BITMAP;
	req->backend = backend;
	req->flags = AIS_PORT_STORE_REQUEST_FLAG_SHORT;

	req->start = csp_hton32(start);
	req->frames_per_reply = frames_per_reply;
	req->delay = 0;
	memcpy(req->bitmap, bitmap, sizeof(req->bitmap));

	packet->length = sizeof(*req);

	/* Calculated expected reply frames */
	count = array_weight(bitmap, sizeof(req->bitmap));
	frames = (count + frames_per_reply - 1) / frames_per_reply;

	conn = csp_connect(CSP_PRIO_NORM, node, AIS_PORT_STORE_REQUEST, timeout, CSP_O_NONE);
	if (!conn) {
		csp_buffer_free(packet);
		return -ECONNREFUSED;
	}

	if (!csp_send(conn, packet, timeout)) {
		csp_buffer_free(packet);
		csp_close(conn);
		return -EIO;
	}

	for (i = 0; i < frames; i++) {
		packet = csp_read(conn, timeout);
		if (!packet) {
			csp_close(conn);
			return -ETIMEDOUT;
		}

		rep = (ais_port_ais_short_rep_t *) packet->data;

		rep->seq_nr = csp_ntoh32(rep->seq_nr);
		seq_nr = rep->seq_nr;

		for (j = 0; j < frames_per_reply; j++) {
			if (rep->frame[j].mmsi != 0xffffffff) {
				rep->frame[j].mmsi = csp_ntoh32(rep->frame[j].mmsi);
				rep->frame[j].time = csp_ntoh32(rep->frame[j].time);
				rep->frame[j].lat = csp_ntoh32(rep->frame[j].lat);
				rep->frame[j].lon = csp_ntoh32(rep->frame[j].lon);
				callback(seq_nr - j, &rep->frame[j]);
			}
		}

		csp_buffer_free(packet);
	}

	csp_close(conn);

	return 0;
}

int ais_request_bitmap_static(int node, uint32_t timeout, uint8_t backend,
			      uint32_t start, uint8_t frames_per_reply,
			      uint8_t bitmap[AIS_PORT_STORE_REQUEST_BITMAP_SIZE],
			      staticmessagecb callback)
{
	int i, j, count, frames;
	ais_port_store_request_bitmap_req_t *req;
	ais_port_ais_static_rep_t *rep;
	csp_packet_t *packet;
	csp_conn_t *conn;

	packet = csp_buffer_get(sizeof(*req));
	if (!packet)
		return -ENOMEM;

	req = (ais_port_store_request_bitmap_req_t *) packet->data;
	req->type = AIS_PORT_STORE_REQUEST_BITMAP;
	req->backend = backend;
	req->flags = 0;

	req->start = csp_hton32(start);
	req->frames_per_reply = frames_per_reply;
	req->delay = 0;
	memcpy(req->bitmap, bitmap, sizeof(req->bitmap));

	packet->length = sizeof(*req);

	/* Calculated expected reply frames */
	count = array_weight(bitmap, sizeof(req->bitmap));
	frames = (count + frames_per_reply - 1) / frames_per_reply;

	conn = csp_connect(CSP_PRIO_NORM, node, AIS_PORT_STORE_REQUEST, timeout, CSP_O_NONE);
	if (!conn) {
		csp_buffer_free(packet);
		return -ECONNREFUSED;
	}

	if (!csp_send(conn, packet, timeout)) {
		csp_buffer_free(packet);
		csp_close(conn);
		return -EIO;
	}

	for (i = 0; i < frames; i++) {
		packet = csp_read(conn, timeout);
		if (!packet) {
			csp_close(conn);
			return -ETIMEDOUT;
		}

		rep = (ais_port_ais_static_rep_t *) packet->data;

		rep->seq_nr = csp_ntoh32(rep->seq_nr);

		for (j = 0; j < frames_per_reply; j++) {
			if (rep->frame[j].seq_nr != 0xffffffff) {
				rep->frame[j].seq_nr = csp_ntoh32(rep->frame[j].seq_nr);
				rep->frame[j].time = csp_ntoh32(rep->frame[j].time);
				callback(&rep->frame[j]);
			}
		}

		csp_buffer_free(packet);
	}

	csp_close(conn);

	return 0;
}

/*
 * AIS_PORT_TIME_CONFIG
 */
int ais_set_time(int node, uint32_t timeout, uint32_t sec, uint32_t nsec) 
{
	ais_port_time_config_set_time_req_t req;
	ais_port_time_config_set_time_rep_t rep;

	req.type = AIS_PORT_TIME_CONFIG_SET_TIME;
	req.sec = csp_hton32(sec);
	req.nsec = csp_hton32(nsec);

	if (csp_transaction(CSP_PRIO_NORM, node, AIS_PORT_TIME_CONFIG, timeout,
			    &req, sizeof(req), &rep, sizeof(rep)) != sizeof(rep))
		return -EINVAL;

	if (rep.error != 0)
		return -EINVAL;

	return 0;
}

int ais_get_time(int node, uint32_t timeout, uint32_t *sec, uint32_t *nsec) 
{
	ais_port_time_config_get_time_req_t req;
	ais_port_time_config_get_time_rep_t rep;

	req.type = AIS_PORT_TIME_CONFIG_GET_TIME;

	if (csp_transaction(CSP_PRIO_NORM, node, AIS_PORT_TIME_CONFIG, timeout,
			    &req, sizeof(req), &rep, sizeof(rep)) != sizeof(rep))
		return -EINVAL;

	if (rep.error != 0) {
		*sec = 0;
		*nsec = 0;
		return -EINVAL;
	}

	*sec = csp_ntoh32(rep.sec);
	*nsec = csp_ntoh32(rep.nsec);

	return 0;
}

int ais_demod_user(int node, uint32_t timeout,
		   uint8_t *tx, size_t txlen,
		   uint8_t *rx, size_t rxlen)
{
	if (csp_transaction(CSP_PRIO_NORM, node,
			    AIS_PORT_DEMOD_USER, timeout,
			    tx, txlen, rx, rxlen) != rxlen) {
		return -EINVAL;
	}

	return 0;
}

int ais_demod_set(int node, uint32_t timeout, char *path, uint16_t boots)
{
	ais_port_demod_config_t req;
	ais_port_demod_config_t rep;

	req.type = AIS_PORT_DEMOD_SET;
	req.boots = csp_hton16(boots);
	strncpy(req.path, path, sizeof(req.path));
	req.path[sizeof(req.path) - 1] = '\0';

	if (csp_transaction(CSP_PRIO_NORM, node,
			    AIS_PORT_DEMOD, timeout,
			    &req, sizeof(req), &rep, sizeof(rep)) != sizeof(rep)) {
		return -EINVAL;
	}

	return 0;
}

int ais_demod_get(int node, uint32_t timeout, char *path, uint16_t *boots)
{
	ais_port_demod_config_t req;
	ais_port_demod_config_t rep;

	req.type = AIS_PORT_DEMOD_GET;
	req.boots = 0;
	memset(req.path, 0, sizeof(req.path));

	if (csp_transaction(CSP_PRIO_NORM, node,
			    AIS_PORT_DEMOD, timeout,
			    &req, sizeof(req), &rep, sizeof(rep)) != sizeof(rep)) {
		return -EINVAL;
	}

	*boots = csp_ntoh16(rep.boots);
	rep.path[sizeof(rep.path) - 1] = '\0';
	strncpy(path, rep.path, sizeof(rep.path));

	return 0;
}

/* ais_channel_set() - switch between AIS channels
 * @node:	CSP address of AIS node
 * @timeout:	Command timeout in ms
 * @channels:	Selected channels
 * @flags:	Commands flags. See description below
 *
 * This command switches between AIS channels 1 and 2 (162.0 MHz) or channels 3
 * and 4 (156.8 Mhz)
 *
 * Return: 0 on success, error code on error.
 */
int ais_channels_set(int node, uint32_t timeout, uint8_t channels, uint32_t flags)
{
	ais_port_channel_config_t req;
	ais_port_channel_config_t rep;

	req.type = AIS_PORT_CHANNEL_SET;
	req.error = 0;
	req.channels = channels;
	req.flags = csp_hton32(flags);

	if (csp_transaction(CSP_PRIO_NORM, node,
			    AIS_PORT_CHANNEL_CONFIG, timeout,
			    &req, sizeof(req), &rep, sizeof(rep)) != sizeof(rep)) {
		return -EINVAL;
	}

	return -rep.error;
}

/* ais_channel_get() - get current channel configuration
 * @node:	CSP address of AIS node
 * @timeout:	Command timeout in ms
 * @channels:	Pointer for channels return value
 *
 * This command switches between AIS channels 1 and 2 (162.0 MHz) or channels 3
 * and 4 (156.8 Mhz)
 *
 * Return: 0 on success, error code on error.
 */
int ais_channels_get(int node, uint32_t timeout, uint8_t *channels)
{
	ais_port_channel_config_t req;
	ais_port_channel_config_t rep;

	req.type = AIS_PORT_CHANNEL_GET;
	req.error = 0;
	req.channels = 0;
	req.flags = csp_hton32(0);

	if (csp_transaction(CSP_PRIO_NORM, node,
			    AIS_PORT_CHANNEL_CONFIG, timeout,
			    &req, sizeof(req), &rep, sizeof(rep)) != sizeof(rep)) {
		return -EINVAL;
	}

	*channels = rep.channels;

	return -rep.error;
}

/* ais_get_version() - get current software version
 * @node:	CSP address of AIS node
 * @timeout:	Command timeout in ms
 * @version:	Pointer to char array where version is stored as text (5 bytes)
 *
 * Return: 0 on success, error code on error
 */
int ais_get_version(int node, uint32_t timeout, char *version)
{
	uint8_t dummy = 0;
	char version_raw[5];

	if (csp_transaction(CSP_PRIO_NORM, node,
			    AIS_PORT_VERSION, timeout,
			    &dummy, sizeof(dummy), version_raw,
			    sizeof(version_raw)) != sizeof(version_raw)) {
		return -EINVAL;
	}

	version_raw[sizeof(version_raw) - 1] = '\0';
	strcpy(version, version_raw);

	return 0;
}

/* ais_eth_set() - enable or disable ethernet
 * @node:	CSP address of AIS node
 * @timeout:	Command timeout in ms
 * @enable:	Set to non-zero value to enable, zero to disable.
 * @boots:	Configuration is valid for this many boots. (0 = do not store, 0xffff = infinite boots).
 *
 * This command enabled ethernet on supported boards.
 *
 * Return: 0 on success, error code on error.
 */
int ais_eth_set(int node, uint32_t timeout, uint8_t enable, uint16_t boots)
{
	ais_port_eth_config_t req;
	ais_port_eth_config_t rep;

	req.type = AIS_PORT_ETH_SET;
	req.error = 0;
	req.enable = enable;
	req.boots = csp_hton16(boots);

	if (csp_transaction(CSP_PRIO_NORM, node,
			    AIS_PORT_ETH_CONFIG, timeout,
			    &req, sizeof(req), &rep, sizeof(rep)) != sizeof(rep)) {
		return -EINVAL;
	}

	return -rep.error;
}

/* ais_eth_get() - get current ethernet configuration
 * @node:	CSP address of AIS node
 * @timeout:	Command timeout in ms
 * @enable:	Pointer for enable return value
 * @boots:	Pointer for boots return value
 *
 * Get the current ethernet configuration
 *
 * Return: 0 on success, error code on error.
 */
int ais_eth_get(int node, uint32_t timeout, uint8_t *enable, uint16_t *boots)
{
	ais_port_eth_config_t req;
	ais_port_eth_config_t rep;

	req.type = AIS_PORT_ETH_GET;
	req.error = 0;
	req.enable = 0;
	req.boots = csp_hton16(0);

	if (csp_transaction(CSP_PRIO_NORM, node,
			    AIS_PORT_ETH_CONFIG, timeout,
			    &req, sizeof(req), &rep, sizeof(rep)) != sizeof(rep)) {
		return -EINVAL;
	}

	*enable = rep.enable;
	*boots = csp_ntoh16(rep.boots);

	return -rep.error;
}

