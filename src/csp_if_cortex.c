#include "csp_if_cortex.h"

#include <stdio.h>
#include <stdlib.h>
#include <endian.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <slash/slash.h>
#include <slash/dflopt.h>
#include <slash/optparse.h>

#include <param/param.h>
#include <vmem/vmem_file.h>

#include <csp/csp.h>
#include <csp/csp_id.h>
#include <csp/csp_crc32.h>
#include <csp/csp_cmp.h>
#include <csp/csp_hooks.h>

#include "param_config.h"
#include "rs.h"
#include "ccsds_randomize.h"

static const int CCSDS_LEN = 219;

typedef struct {
    uint32_t ccsds_asm;         //! CCSDS ASM is 0x1ACFFC1D
    uint8_t idx;                //! Space Inventor index
    uint8_t sequence_number;    //! Space Inventor Radio Sequence number
    uint16_t data_length;       //! Data length in RS frame in bytes
    uint8_t csp_packet[];
} frame_hdr_t;

/** CRT TELEMETRY DATA PACKET FORMAT: Table 5-1 */
typedef struct {
    uint32_t start_of_message;  //! Start of message is 0x499602D2
    uint32_t length;            //! Size of message 4xN
    uint32_t flowid;            //! Always 0
    uint32_t time_seconds;      //! Time in seconds of the current year (weird format)
    uint32_t time_subseconds;   //! Integer or double...
    uint32_t sequence_number;   //! Cortex sequence number
    uint32_t decoding_status;   //! Frame check field
    uint32_t lock_status;       //! 0: No lock, 1: ON lock, 2, ON flywheel, 4: End of data (offline mode)
    uint32_t bit_slip_status;   //! 1: Bit slip occured, 0: No bit slip occured
    uint32_t tm_delay;          //! 0 for "live" telemetry
    uint32_t frame_length;      //! Length of frame in bytes Zero padded bytes not included
    uint32_t sync_word_len;     //! in bits. 8 to 64 bits
    uint32_t frame_check_type;  //! bit 0 RS, bit 1 CRC, bit 2 checksum ... and more
    uint32_t data_format;       //! bit 0 = 1 RS codeblock included, or 0 RS codeblock removed. Bit 1 = corrected errors available
    uint32_t unused1;           //! Reserved
    uint32_t unused2;           //! Reserved
    frame_hdr_t data;           //! First data frame field
} cortex_hdr_tm_t;

typedef struct __attribute__((packed)) {
    uint32_t ccsds_asm;         //! CCSDS ASM is 0x1ACFFC1D
    uint8_t idx;                //! Space Inventor index
    uint8_t sequence_number;    //! Space Inventor Radio Sequence number
    uint16_t data_length;       //! Data length in RS frame in bytes
    uint8_t csp_packet[RS_BLOCK_LENGTH-4-RS_CHECK_LENGTH];
    uint8_t rs_checksum[RS_CHECK_LENGTH];
} ccsds_frame_t ;

/** CRT TELECOMMAND DATA PACKET FORMAT: Table 6 */
typedef struct {
    uint32_t start_of_message;  //! Start of message is 0x499602D2
    uint32_t length;            //! Size of message 4xN
    uint32_t flowid;            //! Always 0
    uint32_t opcode;            //! Always 1
    uint32_t command_tag;       //! Tag for logging purposes
    uint32_t frame_length;      //! Number of bits in transmission
} cortex_hdr_tc_t;

typedef struct {
    uint32_t start_of_message;  //! Start of message is 0x499602D2
    uint32_t length;            //! Size of message 4xN
    uint32_t flowid;            //! Always 0
    uint32_t opcode;            //! Always 1
    uint32_t data;              //! Not specified
    uint32_t time_seconds;      //! Time in seconds of the current year (weird format)
    uint32_t time_subseconds;   //! Integer or double...
    uint32_t status;            //! Cortex sequence number
    uint32_t crc;               //! Frame check field
    uint32_t end_of_message;    //! 0: No lock, 1: ON lock, 2, ON flywheel, 4: End of data (offline mode)
} cortex_tc_ack_t;

typedef struct {
    int32_t crc;
    uint32_t end_of_message;
} cortex_ftr_t;

static uint8_t ringbuf[400000000];
static int ringbuf_write = 0;
static int ringbuf_read = 0;

static int min(int a, int b) {
    if (a < b) return a;
    else return b;
}

static int get_num_frames(int packet_len) {
    return (packet_len / CCSDS_LEN) + 1;
}

static uint32_t cortex_checksum(uint32_t * data, size_t data_size) {
	uint32_t sum = 0;
	for (size_t i = 0; i < data_size / sizeof(uint32_t); i++) {
		sum += be32toh(data[i]);
	}
	return sum;
}

static uint8_t _cortex_dbg = 0;
PARAM_DEFINE_STATIC_RAM(PARAMID_CORTEX_DEBUG, cortex_dbg, PARAM_TYPE_UINT8,  1, 0, PM_DEBUG, NULL, "", &_cortex_dbg, NULL);

static uint8_t _cortex_fwd = 1;
PARAM_DEFINE_STATIC_RAM(PARAMID_CORTEX_FWD, cortex_fwd, PARAM_TYPE_UINT8,  1, 0, PM_CONF, NULL, "", &_cortex_fwd, NULL);

void * cortex_parser_task(void * param) {

    csp_iface_t * iface = param;
    
    csp_packet_t * rx_packet = NULL;
    uint8_t last_idx = -1;
    uint8_t last_seq = -1;

	while(1) {

        /* Calculate current fill level */
        int fill_level = ringbuf_write - ringbuf_read;
        if (fill_level < 0) {
            fill_level += sizeof(ringbuf);
        }

        if (_cortex_dbg & 0x8)
            printf("Read %d, write %d, Fill level %d\n", ringbuf_read, ringbuf_write, fill_level);

        /* Wait for at least one frame */
        if (fill_level < sizeof(cortex_hdr_tm_t)) {
            usleep(1000);
            continue;
        }

        cortex_hdr_tm_t * hdr = (cortex_hdr_tm_t *) &ringbuf[ringbuf_read];

        /* Note 4: Telemetry frames and blocks are 32-bit aligned 
         * (LSBs of the last word are zero-filled if the frame or block length, in bytes, is not a multiple of 4).*/
        if (hdr->start_of_message != htobe32(0x499602D2)) {
            ringbuf_read = (ringbuf_read + 1) % sizeof(ringbuf);
            printf("sidestep in ringbuf at %d\n", ringbuf_read);
            continue;
        }

        uint32_t ctx_seq = be32toh(hdr->sequence_number);
        uint32_t ctx_len = be32toh(hdr->length);
        uint32_t ctx_frame_len = be32toh(hdr->frame_length);
        uint32_t ctx_rs_status = be32toh(hdr->decoding_status);
        uint32_t ctx_lock = be32toh(hdr->lock_status);
        time_t ctx_time = be32toh(hdr->time_seconds) + 1672527600 + 3600; // Cortex logs timestamp relative to Jan 1st 2023
        struct tm *tmp = gmtime(&ctx_time);
        int ok = (ctx_rs_status == 0);

        int rs_corrected = -1;
        if ((be32toh(hdr->data_format) & 0x2) && (be32toh(hdr->frame_check_type) & 0x1)) {
            rs_corrected = (ctx_rs_status && 0xFF00) >> 8;
        }

        if (_cortex_dbg & 0x4)
            printf("CORTEX seq: %d, len %d (payload %d), ok %d (rserr %d), lock %d, %02d-%02d-%04d %02d:%02d:%02d\n", ctx_seq, ctx_len, ctx_frame_len, ok, rs_corrected, ctx_lock, tmp->tm_mday, tmp->tm_mon + 1, tmp->tm_year + 1900, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

        /* Frame sanity checks */
        if (_cortex_dbg & 0x1) {
            if (be32toh(hdr->bit_slip_status) != 0)
                printf("WARNING: BIT SLIP\n");
            if (be32toh(hdr->tm_delay) != 0)
                printf("WARNING: TM DELAY\n");
            if (be32toh(hdr->frame_check_type) != 1)
                printf("WARNING: Non RS frame\n");
            if (be32toh(hdr->sync_word_len) != 32)
                printf("WARNING: Non 32-bit sync word\n");
        }

        iface->frame++;

        /* Skip frames with RS Check error */
        if (!ok) {
            iface->rx_error++;
            goto skip;
        }

        /* Expect CCSDS ASM */
        if (be32toh(hdr->data.ccsds_asm) != 0x1ACFFC1D) {
            if (_cortex_dbg & 0x1)
                printf("WARNING: Non CCSDS frame\n");
            goto skip;
        }

        uint8_t idx = hdr->data.idx;
        uint8_t seq = hdr->data.sequence_number;
        uint16_t len = be16toh(hdr->data.data_length);

        /* Skip empty (IDLE) frames */
        if ((len == 0) || (len > 2000))
            goto skip;

        if (_cortex_dbg & 0x2)
            printf("FRAME: idx %u, seq %u, len %u\n", idx, seq, len);

        /* Multiple frames in a single CSP packet support */
        uint8_t numframes = get_num_frames(len);

        /* Beginning of new CSP packet, reset check variables */
        if (idx == 0) {
            last_seq = seq;
            last_idx = 0;
        /* Check for CCSDS frame loss */
        } else if (last_seq != seq || ++last_idx != idx) {
            printf("Discarding packet due to %u != %u || ++%u != %u \n", last_seq, seq, last_idx, idx);
            csp_buffer_free(rx_packet);
            rx_packet = NULL;
            goto skip;
        }

        if(numframes > idx + 1) {
            if (_cortex_dbg & 0x10)
                printf("Found frame %d of %d with len %d, waiting for next one\n", idx, numframes, len);
            goto skip;
        }

        /* For dry runs, we have done enough now */
        if (_cortex_fwd == 0)
            goto skip;

        /* Allocate CSP packet buffer */
        if (rx_packet == NULL) {
            if (csp_buffer_remaining() < 10) {
                usleep(1);
            }
            while((rx_packet = csp_buffer_get(0)) == NULL) {
                usleep(1);
            }
            csp_id_setup_rx(rx_packet);
        }

        /* Move data to CSP buffer (with support for spanning multiple frames)*/
        rx_packet->frame_length = len;
        memcpy(&rx_packet->frame_begin[idx * CCSDS_LEN], &hdr->data.csp_packet, min(len-idx*CCSDS_LEN,CCSDS_LEN));

        /* Parse CSP header */
        if (csp_id_strip(rx_packet) < 0) {
            csp_buffer_free(rx_packet);
            rx_packet = NULL;
            goto skip;
        }

        /* Send back into CSP, notice calling from task so last argument must be NULL! */
        rx_packet->timestamp_rx = ctx_time;
        csp_qfifo_write(rx_packet, iface, NULL);
        rx_packet = NULL;

skip:
        ringbuf_read = (ringbuf_read + ctx_len) % sizeof(ringbuf);
    }
	return NULL;
}

static int cortex_file(struct slash *slash) {

	char * filename = NULL;

    optparse_t * parser = optparse_new("cortex file", "<filename>");
    optparse_add_help(parser);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
	    return SLASH_EINVAL;
    }

    filename = slash->argv[argi];
    FILE * file = fopen(filename, "rb");

    if (file == NULL) {
        printf("File could not be opened\n");
        return SLASH_EINVAL;
    }

    while(1) {

        /* Calculate current fill level */
        int fill_level = ringbuf_write - ringbuf_read;
        if (fill_level < 0) {
            fill_level += sizeof(ringbuf);
        }
        /* Remain is -1 because we dont want buffer to overwrite itself */
        int remain = sizeof(ringbuf) - fill_level - 1;

        printf("Remain %d\n", remain);
        if (remain == 0) {
            break;
        }

        int buf_end = sizeof(ringbuf) - ringbuf_write;

        int filesize = fread(&ringbuf[ringbuf_write], 1, min(remain, buf_end), file);
        if (filesize == 0) {
            break;
        }
        ringbuf_write = (ringbuf_write + filesize) % sizeof(ringbuf);
        printf("Read %d bytes from %s\n", filesize, filename);
    }
    
    fclose(file);

    return SLASH_SUCCESS;
}

slash_command_sub(cortex, file, cortex_file, NULL, NULL);

/* CRT TLM request header, configured for telemetry channel B with permanent flow */
static uint8_t tlm_req[] = {0x49, 0x96, 0x02, 0xd2, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb6, 0x69, 0xfd, 0x2e};

void * csp_if_cortex_rx_task(void * param) {

    csp_iface_t * iface = param;
	csp_if_cortex_conf_t * ifconf = iface->driver_data;
    int fd;
    int fill_level, remain, buf_end;
    ssize_t valread;

new_connection:
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

connect:
    ifconf->cortex_ip.sin_family = AF_INET;
    ifconf->cortex_ip.sin_port = htons(ifconf->rx_port);
    if (connect(fd, (struct sockaddr *) &ifconf->cortex_ip, sizeof(ifconf->cortex_ip)) < 0) {
        printf("TM connection error: %s\n", strerror(errno));
        sleep(1);
        goto connect;
    }

    /* Send TLM request to open Cortex channel */
    if (send(fd, &tlm_req, sizeof(tlm_req), MSG_NOSIGNAL) <= 0) {
        printf("TM send error: %s\n", strerror(errno));
        close(fd);
        goto new_connection;
    }

read:
    /* Calculate current fill level */
    fill_level = ringbuf_write - ringbuf_read;
    if (fill_level < 0) {
        fill_level += sizeof(ringbuf);
    }

    /* Remain is -1 because we dont want buffer to overwrite itself */
    remain = sizeof(ringbuf) - fill_level - 1;
    if (remain == 0) {
        printf("TM buffer full\n");
        sleep(1);
        goto read;
    }

    buf_end = sizeof(ringbuf) - ringbuf_write;

    /* Blocking read */
    valread = recv(fd, &ringbuf[ringbuf_write], min(remain, buf_end), MSG_NOSIGNAL);
    if (valread <= 0) {
        printf("TM read error: %s\n", strerror(errno));
        close(fd);
        goto new_connection;
    }

    ringbuf_write = (ringbuf_write + valread) % sizeof(ringbuf);
    printf("TM received %ld bytes\n", valread);
    goto read;

    return NULL;
}

static int csp_if_cortex_tx(csp_iface_t * iface, uint16_t via, csp_packet_t * packet, int from_me) {

    static int fd = -1;
    static uint8_t seq_num = 0;
    csp_if_cortex_conf_t * ifconf = iface->driver_data;
    
    csp_id_prepend(packet);
    printf("TC packet len %d, frame len %d\n", packet->length, packet->frame_length);
    csp_hex_dump("frame", packet->frame_begin, packet->frame_length);

    uint8_t frame_buffer[3000]; // Holds a 2kb CSP packet with RS checksum and ASM, along with Cortex header
    cortex_hdr_tc_t* cortex_frame = (cortex_hdr_tc_t*)&frame_buffer[0];
    cortex_frame->start_of_message = htobe32(1234567890);
    cortex_frame->flowid = 0;
    cortex_frame->opcode = htobe32(1);
    cortex_frame->command_tag = 0;

    int len_total = sizeof(cortex_hdr_tc_t);
    int len_payload = 0;

    uint8_t numframes = get_num_frames(packet->frame_length);

    for (int idx = 0; idx < numframes; idx++) {

        ccsds_frame_t* ccsds_frame = (ccsds_frame_t*)&frame_buffer[sizeof(cortex_hdr_tc_t) + idx*sizeof(ccsds_frame_t)];
        ccsds_frame->ccsds_asm = htobe32(0x1ACFFC1D);
        ccsds_frame->data_length = packet->frame_length;
        ccsds_frame->sequence_number = seq_num++;
        ccsds_frame->idx = idx;

        memcpy(ccsds_frame->csp_packet, &packet->frame_begin[idx*CCSDS_LEN], min(packet->frame_length-idx*CCSDS_LEN, CCSDS_LEN));
        encode_rs_8(ccsds_frame->csp_packet, ccsds_frame->rs_checksum, 0);
        ccsds_randomize(ccsds_frame->csp_packet);

        len_total += sizeof(ccsds_frame_t);
        len_payload += sizeof(ccsds_frame_t);
    }

    /* Cortex require length of frame to be a multiplum of 4-byte words */
	len_total += (sizeof(ccsds_frame_t)*numframes % 4 ?  4 - (sizeof(ccsds_frame_t)*numframes % 4) : 0);

    cortex_ftr_t* cortex_ftr = (cortex_ftr_t*)&frame_buffer[len_total];
    len_total += sizeof(cortex_ftr_t);

    cortex_ftr->crc = 0; // CRC field is part of checksum calculation, so set to 0 before calculation
    cortex_ftr->end_of_message = htobe32(-1234567890);
    cortex_frame->frame_length = htobe32(8 * len_payload);
    cortex_frame->length = htobe32(len_total);
    uint32_t checksum = cortex_checksum((uint32_t *) cortex_frame, len_total);
    cortex_ftr->crc = htobe32(-checksum);

    csp_hex_dump("frame", frame_buffer, len_total);

    checksum = cortex_checksum((uint32_t *) frame_buffer, len_total);
    printf("Own cortex checksum %x\n", checksum);

    if (fd < 0) {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        ifconf->cortex_ip.sin_family = AF_INET;
        ifconf->cortex_ip.sin_port = htons(ifconf->tx_port);
        if (connect(fd, (struct sockaddr *) &ifconf->cortex_ip, sizeof(ifconf->cortex_ip)) < 0) {
            printf("TC connect error: %s\n", strerror(errno));
            close(fd);
            fd = -1;
            csp_buffer_free(packet);
            return CSP_ERR_TX;
        }
        printf("TC connection open\n");
    }

    printf("TC sending %d bytes\n", len_total);
    int sentbytes = send(fd, frame_buffer, len_total, MSG_NOSIGNAL);

    if (sentbytes <= 0) {
        printf("TC send error: %s\n", strerror(errno));
        close(fd);
        fd = -1;
        csp_buffer_free(packet);
        return CSP_ERR_TX;
    }

    /* Get ack */
    int recvbytes = recv(fd, frame_buffer, sizeof(frame_buffer), MSG_NOSIGNAL | MSG_PEEK | MSG_DONTWAIT);
    if (recvbytes < 0) {
        printf("TC receive error: %s\n", strerror(errno));
        goto out;
    }

    csp_hex_dump("TC response: ", frame_buffer, recvbytes);

#if 0
    cortex_tc_ack_t* cortex_ack = (cortex_tc_ack_t*)&frame_buffer[0];
    if (cortex_ack->status == 0) {
        csp_buffer_free(packet);
        return CSP_ERR_TX;
    }
#endif

out:
    csp_buffer_free(packet);
    return CSP_ERR_NONE;

}

void csp_if_cortex_init(csp_iface_t * iface, csp_if_cortex_conf_t * ifconf) {

	iface->driver_data = ifconf;

	if (inet_aton(ifconf->host, &ifconf->cortex_ip.sin_addr) == 0) {
		csp_print("  Unknown peer address %s\n", ifconf->host);
	}

	csp_print("  Cortex peer address: %s, rx-port: %d, tx-port %d\n", inet_ntoa(ifconf->cortex_ip.sin_addr), ifconf->rx_port, ifconf->tx_port);

    pthread_attr_t attributes;
	int ret;
	/* Start server thread */
	ret = pthread_attr_init(&attributes);
	if (ret != 0) {
		csp_print("csp_if_udp_init: pthread_attr_init failed: %s: %d\n", strerror(ret), ret);
	}
	ret = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
	if (ret != 0) {
		csp_print("csp_if_udp_init: pthread_attr_setdetachstate failed: %s: %d\n", strerror(ret), ret);
	}
	ret = pthread_create(&ifconf->rx_task, &attributes, &csp_if_cortex_rx_task, iface);
    ret = pthread_create(&ifconf->parser_task, &attributes, &cortex_parser_task, iface);

	/* Regsiter interface */
	iface->nexthop = csp_if_cortex_tx,
	csp_iflist_add(iface);

}

