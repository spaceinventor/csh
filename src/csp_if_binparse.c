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

static csp_iface_t iface;

time_t binparse_time;

static bool initialized = false;

int min(int a, int b) {

    if (a < b) return a;
    else return b;
}

static int csp_if_binparse(struct slash *slash) {

    if(!initialized) {
        iface.is_default = true;
        iface.addr = 1;
        iface.netmask = 8;
        iface.name = "BINPARSE";

        initialized = true;
    }

	char * filename = NULL;
    int sim = 0;

    optparse_t * parser = optparse_new("binparse", "filename");
    optparse_add_help(parser);
    optparse_add_set(parser, 's', "sim", 1, &sim, "Simulation Mode");


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
    const int MAX_LEN = 2000;
    const int CCSDS_LEN = 219;

    uint8_t * d = malloc(MAX_SIZE);
    uint8_t * dd = d;

    int filesize = fread((void*)d, 1, MAX_SIZE, file);
    printf("Read %d bytes from %s\n", filesize, filename);

    int cnt = 0;
    //int lastseq = -1;

    csp_packet_t * rx_packet = NULL;

    while(d-dd < filesize) {

        if (d[0] == 0x49 && d[1] == 0x96 && d[2] == 0x02 && d[3] == 0xD2
            && d[16*4+0] == 0x1A && d[16*4+1] == 0xCF && d[16*4+2] == 0xFC && d[16*4+3] == 0x1D) {

            /* RS decoding status */
            if (be32toh(*(uint32_t*)&d[6*4]) != 0) {
                // printf("Failed RS check\n");
                d += 82*4;
                continue;
            }
                
            binparse_time = be32toh(*(uint32_t*)&d[3*4])+1672527600 + 3600; // Cortex logs timestamp relative to Jan 1st 2023

            uint8_t idx = d[17*4];
            //uint8_t seq = d[17*4+1];
            uint16_t len = be16toh(*(uint16_t*)(&d[17*4+2]));

            if(len >= 10 && len <= MAX_LEN) {

                if (rx_packet == NULL) {
                    rx_packet = csp_buffer_get(0);
                    csp_id_setup_rx(rx_packet);
                }

                rx_packet->frame_length = len;
                memcpy(&rx_packet->frame_begin[idx*CCSDS_LEN], &d[18*4], min(len-idx*CCSDS_LEN,CCSDS_LEN));

                int numframes = len/CCSDS_LEN;
                if(numframes > idx+1) {
                    printf("Found frame %d of %d with len %d, looking for next one\n", idx, len, numframes);
                    d += 82*4;
                    continue;
                }
                if (idx > 0) printf("Found all frames, proceeding\n");

                if (csp_id_strip(rx_packet) < 0) {
                    csp_buffer_free(rx_packet);
                    rx_packet = NULL;
                    printf("Failed ID strip\n");
                    d += 82*4;
                    continue;
                }

                /* CRC32 verified packet */
                // if (rx_packet->id.flags & CSP_FCRC32) {
                //     if (rx_packet->length < 4) {
                //         printf("Too short packet for CRC32, %u\n", rx_packet->length);
                //         csp_buffer_free(rx_packet);
                //         continue;
                //     }
                //     // rx_packet->length += 4;
                //     /* Verify CRC32 (does not include header for backwards compatability with csp1.x) */
                //     if (csp_crc32_verify(rx_packet) != 0) {
                //         /* Checksum failed */
                //         printf("CRC32 verification error! Discarding packet\n");
                //         csp_buffer_free(rx_packet);
                //         continue;
                //     }
                // }

                // if (rx_packet->id.src == 331) {
                //     printf("Found HK with length %u on port %u\n", len, rx_packet->id.sport);
                // }

                // printf("Found packet with length %d port %d from node %d\n", len, rx_packet->id.sport, rx_packet->id.src);

                struct tm *tmp = gmtime(&binparse_time);
                printf("%02d-%02d-%04d %02d:%02d:%02d\n", tmp->tm_mday, tmp->tm_mon + 1, tmp->tm_year + 1900, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

                /* Send back into CSP, notice calling from task so last argument must be NULL! */
                if (sim) {
                    printf("SIM: %lu\n", binparse_time);
                    csp_input_hook(&iface, rx_packet);
                    csp_buffer_free(rx_packet);
                    rx_packet = NULL;
                } else {
                    csp_qfifo_write(rx_packet, &iface, NULL);
                    rx_packet = NULL;
                    usleep(10000);
                }

                cnt++;
            }
            d += 82*4;
        } else {
            d += 4;
        }
    }

    printf("Found %d packets\n", cnt);

    free(dd);

    return SLASH_SUCCESS;
}

slash_command(binparse, csp_if_binparse, NULL, NULL);
