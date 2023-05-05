#include <stdio.h>
#include <endian.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <slash/slash.h>
#include <slash/dflopt.h>
#include <slash/optparse.h>

#include <param/param_queue.h>

#include <csp/csp.h>
#include <csp/csp_id.h>
#include <csp/csp_crc32.h>
#include <csp/csp_cmp.h>
#include <csp/csp_hooks.h>

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
} cortex_hdr_t;

static csp_iface_t iface = {
    .name = "BINPARSE",
    .addr = 14000,
    .netmask = 8,
};

int min(int a, int b) {
    if (a < b) return a;
    else return b;
}

static int csp_if_binparse(struct slash *slash) {

	char * filename = NULL;
    int dry_run = 0;
    int ctx_dbg = 0;

    optparse_t * parser = optparse_new("binparse", "filename");
    optparse_add_help(parser);
    optparse_add_set(parser, 'n', "dry-run", 1, &dry_run, "Dry run mode");
    optparse_add_set(parser, 'c', "cortex", 1, &ctx_dbg, "Print cortex debug information");

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

    const int MAX_SIZE = 10*1024*1024;
    const int CCSDS_LEN = 219;

    uint8_t * d = malloc(MAX_SIZE);
    uint8_t * dd = d;

    int filesize = fread((void*)d, 1, MAX_SIZE, file);
    printf("Read %d bytes from %s\n", filesize, filename);

    int cnt = 0;

    csp_packet_t * rx_packet = NULL;

    struct timespec ts_begin;
    clock_gettime(CLOCK_MONOTONIC, &ts_begin);


    while(d-dd < filesize) {

        cortex_hdr_t * hdr = (cortex_hdr_t *) d;

        /* Note 4: Telemetry frames and blocks are 32-bit aligned 
         * (LSBs of the last word are zero-filled if the frame or block length, in bytes, is not a multiple of 4).*/
        if (hdr->start_of_message != htobe32(0x499602D2)) {
            d += 4;
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

        if (ctx_dbg)
            printf("CORTEX seq: %d, len %d (payload %d), ok %d (rserr %d), lock %d, %02d-%02d-%04d %02d:%02d:%02d\n", ctx_seq, ctx_len, ctx_frame_len, ok, rs_corrected, ctx_lock, tmp->tm_mday, tmp->tm_mon + 1, tmp->tm_year + 1900, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

        /* Frame sanity checks */
        if (be32toh(hdr->bit_slip_status) != 0)
            printf("WARNING: BIT SLIP\n");
        if (be32toh(hdr->tm_delay) != 0)
            printf("WARNING: TM DELAY\n");
        if (be32toh(hdr->frame_check_type) != 1)
            printf("WARNING: Non RS frame\n");
        if (be32toh(hdr->sync_word_len) != 32)
            printf("WARNING: Non 32-bit sync word\n");

        /* Skip frames with RS Check error */
        if (!ok)
            goto skip;

        /* Expect CCSDS ASM */
        if (be32toh(hdr->data.ccsds_asm) != 0x1ACFFC1D) {
            printf("WARNING: Non CCSDS frame\n");
            goto skip;
        }

        uint8_t idx = hdr->data.idx;
        uint8_t seq = hdr->data.sequence_number;
        uint16_t len = be16toh(hdr->data.data_length);

        /* Skip empty (IDLE) frames */
        if ((len == 0) || (len > 2000))
            goto skip;

        printf("FRAME: idx %u, seq %u, len %u\n", idx, seq, len);
        cnt++;

        /* For dry runs, we have done enough now */
        if (dry_run)
            goto skip;

        /* Allocate CSP packet buffer */
        if (rx_packet == NULL) {
            while((rx_packet = csp_buffer_get(0)) == NULL) {
                usleep(1);
            }
            csp_id_setup_rx(rx_packet);
        }

        /* Move data to CSP buffer (with support for spanning multiple frames)*/
        rx_packet->frame_length = len;
        memcpy(&rx_packet->frame_begin[idx * CCSDS_LEN], &hdr->data.csp_packet, min(len-idx*CCSDS_LEN,CCSDS_LEN));

        /* Multiple frames in a since CSP packet support */
        int numframes = len / CCSDS_LEN;
        if(numframes > idx + 1) {
            printf("Found frame %d of %d with len %d, looking for next one\n", idx, len, numframes);
            goto skip;
        }
        if (idx > 0)
            printf("Found all frames, proceeding\n");

        /* Parse CSP header */
        if (csp_id_strip(rx_packet) < 0) {
            csp_buffer_free(rx_packet);
            rx_packet = NULL;
            goto skip;
        }

        /* Send back into CSP, notice calling from task so last argument must be NULL! */
        csp_qfifo_write(rx_packet, &iface, NULL);
        rx_packet = NULL;

skip:
        d += ctx_len;

    }

    struct timespec ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_end);

    float duration = (ts_end.tv_sec - ts_begin.tv_sec) + (ts_end.tv_nsec - ts_begin.tv_nsec) / 1e9;

    printf("Processed %d bytes in %f seconds\n", filesize, duration);
    printf("Found %d packets\n", cnt);

    free(dd);

    return SLASH_SUCCESS;
}

slash_command(binparse, csp_if_binparse, NULL, NULL);
