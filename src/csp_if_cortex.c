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
    .name = "CORTEX",
    .addr = 14000,
    .netmask = 8,
};

static uint8_t ringbuf[400000000];
static int ringbuf_write = 0;
static int ringbuf_read = 0;

int min(int a, int b) {
    if (a < b) return a;
    else return b;
}

VMEM_DEFINE_FILE(cortex, "cortex", "cortex.vmem", 2048);

static uint8_t _cortex_dbg = 1;
PARAM_DEFINE_STATIC_RAM(PARAMID_CORTEX_DEBUG, cortex_dbg, PARAM_TYPE_UINT8,  1, 0, PM_DEBUG, NULL, "", &_cortex_dbg, NULL);

static uint8_t _cortex_fwd = 1;
PARAM_DEFINE_STATIC_RAM(PARAMID_CORTEX_FWD, cortex_fwd, PARAM_TYPE_UINT8,  1, 0, PM_CONF, NULL, "", &_cortex_fwd, NULL);

static uint8_t _cortex_parser_en = 0;
void cortex_parser_en_cb(struct param_s * param, int offset) {
    vmem_file_init(&vmem_cortex);
	static pthread_t cortex_handle = 0;
    if (cortex_handle == 0) {
        void *cortex_parser_task(void * param);
	    pthread_create(&cortex_handle, NULL, &cortex_parser_task, NULL);
    }
}
PARAM_DEFINE_STATIC_RAM(PARAMID_CORTEX_PARSER_EN, cortex_parser_en, PARAM_TYPE_UINT8,  1, 0, PM_CONF, cortex_parser_en_cb, "", &_cortex_parser_en, NULL);

PARAM_DEFINE_STATIC_VMEM(PARAMID_CORTEX_IP_UL_S, cortex_ip_ul_s, PARAM_TYPE_STRING, 16, 0, PM_CONF, NULL, "", cortex, 0, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAMID_CORTEX_IP_DL_S, cortex_ip_dl_s, PARAM_TYPE_STRING, 16, 0, PM_CONF, NULL, "", cortex, 1, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAMID_CORTEX_IP_DL_X, cortex_ip_dl_x, PARAM_TYPE_STRING, 16, 0, PM_CONF, NULL, "", cortex, 2, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAMID_CORTEX_PORT_UL_S, cortex_port_ul_s, PARAM_TYPE_UINT16, sizeof(uint16_t), 0, PM_CONF, NULL, "", cortex, 3, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAMID_CORTEX_PORT_DL_S, cortex_port_dl_s, PARAM_TYPE_UINT16, sizeof(uint16_t), 0, PM_CONF, NULL, "", cortex, 4, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAMID_CORTEX_PORT_DL_X, cortex_port_dl_x, PARAM_TYPE_UINT16, sizeof(uint16_t), 0, PM_CONF, NULL, "", cortex, 5, NULL);

void csp_if_cortex_init(csp_iface_t* iface) {

    csp_iflist_add(iface);
}

void * cortex_parser_task(void * param) {

    csp_if_cortex_init(&iface);
    
    csp_packet_t * rx_packet = NULL;
    uint8_t last_idx = -1;
    uint8_t last_seq = -1;

	while(1) {

        if (_cortex_parser_en == 0) {
            sleep(1);
            continue;
        }

        /* Calculate current fill level */
        int fill_level = ringbuf_write - ringbuf_read;
        if (fill_level < 0) {
            fill_level += sizeof(ringbuf);
        }

        if (_cortex_dbg & 0x8)
            printf("Read %d, write %d, Fill level %d\n", ringbuf_read, ringbuf_write, fill_level);

        /* Wait for at least one frame */
        if (fill_level < sizeof(cortex_hdr_t)) {
            usleep(1000);
            continue;
        }

        cortex_hdr_t * hdr = (cortex_hdr_t *) &ringbuf[ringbuf_read];

        /* Note 4: Telemetry frames and blocks are 32-bit aligned 
         * (LSBs of the last word are zero-filled if the frame or block length, in bytes, is not a multiple of 4).*/
        if (hdr->start_of_message != htobe32(0x499602D2)) {
            ringbuf_read = (ringbuf_read + 1) % sizeof(ringbuf);
            printf("sidestep in ringbuf\n");
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

        iface.frame++;

        /* Skip frames with RS Check error */
        if (!ok) {
            iface.rx_error++;
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
        static const int CCSDS_LEN = 219;
        uint8_t numframes = len / CCSDS_LEN;
        if (len * CCSDS_LEN < numframes) numframes++; // ceil(...)

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
        csp_qfifo_write(rx_packet, &iface, NULL);
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

void * cortex_dl_s_task(void * param) {

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        printf("Creating socket for Cortex DL S-band failed with %d\n", errno);
    }

    while (1) {

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

        ssize_t valread = read(fd, &ringbuf[ringbuf_write], min(remain, buf_end));

        /* Socket not currently connected, so let's do that */
        if (valread < 0) {

            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(param_get_uint16(&cortex_port_dl_s));
        
            char ip[16];
            param_get_string(&cortex_ip_dl_s, ip, sizeof(ip));
            if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
                printf("Invalid address or address not supported \n");
            }

            if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                sleep(1);
                continue;
            }

            /* Send TLM request to open Cortex channel */
            write(fd, &tlm_req, sizeof(tlm_req));

            valread = read(fd, &ringbuf[ringbuf_write], min(remain, buf_end));
            if (valread < 0) {
                sleep(1);
                continue;
            }
        }

        ringbuf_write = (ringbuf_write + valread) % sizeof(ringbuf);
        printf("Read %ld bytes from Cortex DL S-band\n", valread);
    }

    return NULL;
}

void cortex_dl_s_init() {

	static pthread_t cortex_handle = 0;
    if (cortex_handle == 0) {
	    pthread_create(&cortex_handle, NULL, &cortex_dl_s_task, NULL);
    }
}

int cortex_ul_s(csp_iface_t * iface, uint16_t via, csp_packet_t * packet, int from_me) {

    /* TX is not yet supported */    
    return -1;
}

static int csp_ifadd_cortex_cmd(struct slash *slash) {

    static int ifidx = 0;

    if (ifidx > 1) {
        printf("Multiple Cortex interfaces are not supported in this version\n");
        return SLASH_ENOMEM;
    }

    char name[10];
    sprintf(name, "Cortex%u", ifidx++);
    
    int promisc = 0;
    int mask = 8;
    int dfl = 0;

    optparse_t * parser = optparse_new("csp add cortex", "<addr>");
    optparse_add_help(parser);
    optparse_add_set(parser, 'p', "promisc", 1, &promisc, "Promiscous Mode");
    optparse_add_int(parser, 'm', "mask", "NUM", 0, &mask, "Netmask (defaults to 8)");
    optparse_add_set(parser, 'd', "default", 1, &dfl, "Set as default");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("missing parameter addr\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char * endptr;
    unsigned int addr = strtoul(slash->argv[argi], &endptr, 10);

    csp_if_cortex_init(&iface);

    if (param_get_uint16(&cortex_port_ul_s) != 0) {
        printf("UL is not yet supported\n");
    }

    if (param_get_uint16(&cortex_port_dl_s) != 0) {
        cortex_dl_s_init();
    }

    if (param_get_uint16(&cortex_port_dl_x) != 0) {
        printf("DL on X-band is not yet supported\n");
    }

    iface.is_default = dfl;
    iface.addr = addr;
	iface.netmask = mask;
    iface.nexthop = cortex_ul_s;

    return SLASH_SUCCESS;
}

slash_command_subsub(csp, add, cortex, csp_ifadd_cortex_cmd, NULL, "Add a new Cortex socket interface");
