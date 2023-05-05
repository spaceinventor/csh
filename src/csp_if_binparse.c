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

    struct timespec ts_begin;
    clock_gettime(CLOCK_MONOTONIC, &ts_begin);

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
                    while((rx_packet = csp_buffer_get(0)) == NULL) {
                        usleep(1);
                    }
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

                struct tm *tmp = gmtime(&binparse_time);
                printf("%02d-%02d-%04d %02d:%02d:%02d\n", tmp->tm_mday, tmp->tm_mon + 1, tmp->tm_year + 1900, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

                /* Send back into CSP, notice calling from task so last argument must be NULL! */
                if (sim) {
                    csp_input_hook(&iface, rx_packet);
                    csp_buffer_free(rx_packet);
                    rx_packet = NULL;
                } else {
                    csp_qfifo_write(rx_packet, &iface, NULL);
                    rx_packet = NULL;
                }

                cnt++;
            }
            d += 82*4;
        } else {
            d += 4;
        }
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
