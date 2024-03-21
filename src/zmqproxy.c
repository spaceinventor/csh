/*
Cubesat Space Protocol - A small network-layer protocol designed for Cubesats
Copyright (C) 2012 GomSpace ApS (http://www.gomspace.com)
Copyright (C) 2012 AAUSAT3 Project (http://aausat3.space.aau.dk)

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <unistd.h>
#include <stdlib.h>
#include <zmq.h>
#include <assert.h>
#include <pthread.h>

#include <csp/csp.h>
#include <csp/interfaces/csp_if_zmqhub.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#define CURVE_KEYLEN 41
// #define ZMQ_PROXY_LOSSY

int csp_id_strip(csp_packet_t * packet);
int csp_id_setup_rx(csp_packet_t * packet);
extern csp_conf_t csp_conf;

#ifdef ZMQ_PROXY_LOSSY
#include <time.h>
#include <csp/arch/csp_time.h>
#include <csp/arch/csp_queue.h>
void zmq_proxy_lossy();

int hdx_powerup_start_ms = 0;
int hdx_node = 0;
int hdx_last_tx_ms = 0;
int hdx_powerup_delay = 30;
int hdx_powerdown_delay = 100;
double loss_prob = 0.0;
double corr_prob = 0.0;
double delay_prob = 0.0;
int enable_lossy = 0;
int delay_ms = 0;
int seed = 0;
char * shortopts = "hagv:d:s:p:f:L:C:D:T:S:N:U:O:";
#else
char * shortopts = "hagv:d:s:p:f:";
#endif

int debug = 0;
char * sub_str = "tcp://0.0.0.0:6000";
char * pub_str = "tcp://0.0.0.0:7000";
void *ctx = NULL;
void *frontend = NULL;
void *backend = NULL;
char * logfile_name = NULL;
FILE * logfile;
/* Auth flag set if -a arg set */
int auth = 0;
/* Buffer to hold the secret key. 41 is the length of a z85-encoded CURVE key plus 1 for the null terminator. */
char sec_key[CURVE_KEYLEN] = {0};

/* Read one event off the monitor socket; return value and address
by reference, if not null, and event number by value. Returns -1
in case of error. */
static int get_monitor_event(void * monitor, int * value, char ** address) {
    // First frame in message contains event number and value
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    if (zmq_msg_recv(&msg, monitor, 0) == -1)
        return -1;  // Interrupted, presumably
    assert(zmq_msg_more(&msg));

    uint8_t * data = (uint8_t *)zmq_msg_data(&msg);
    uint16_t event = *(uint16_t *)(data);
    if (value)
        *value = *(uint32_t *)(data + 2);

    // Second frame in message contains event address
    zmq_msg_init(&msg);
    if (zmq_msg_recv(&msg, monitor, 0) == -1)
        return -1;  // Interrupted, presumably
    assert(!zmq_msg_more(&msg));

    if (address) {
        uint8_t * data = (uint8_t *)zmq_msg_data(&msg);
        size_t size = zmq_msg_size(&msg);
        *address = (char *)malloc(size + 1);
        memcpy(*address, data, size);
        (*address)[size] = 0;
    }
    return event;
}

void handle_event(int event, int value, char *address){

    switch (event) {
        case ZMQ_EVENT_ACCEPTED: {
            int fd = value;
            if (fd != -1) {
                struct sockaddr_storage addr;
                socklen_t len = sizeof(addr);

                if (getpeername(fd, (struct sockaddr *)&addr, &len) != -1) {
                    char ipstr[INET6_ADDRSTRLEN];
                    int port;

                    if (addr.ss_family == AF_INET) {
                        struct sockaddr_in * s = (struct sockaddr_in *)&addr;
                        port = ntohs(s->sin_port);
                        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
                    } else {  // AF_INET6
                        struct sockaddr_in6 * s = (struct sockaddr_in6 *)&addr;
                        port = ntohs(s->sin6_port);
                        inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
                    }
                    printf("%s:%d ", ipstr, port);
                }
            }
            printf("connected on %s\n", address);
            break;
        }
        case ZMQ_EVENT_HANDSHAKE_SUCCEEDED:
            printf("Handshake succeeded on %s\n", address);
            break;
        case ZMQ_EVENT_HANDSHAKE_FAILED_PROTOCOL:
            printf("Handshake protocol failure on %s\n", address);
            break;
        case ZMQ_EVENT_HANDSHAKE_FAILED_AUTH:
            printf("Handshake failed auth on %s\n", address);
            break;
        case ZMQ_EVENT_DISCONNECTED:
            printf("Client disconnected on %s\n", address);
            break;
        case ZMQ_EVENT_HANDSHAKE_FAILED_NO_DETAIL:
            /*  Unspecified system errors during handshake. Event value is an errno.      */
            printf("Unspecified system errors during handshake %s errno: %d\n", address, value);
            break;
        default:
            printf("event: 0x%x\n", event);
            break;
    }
}

static void * task_monitor_backend(void *arg) {
    int rc = zmq_socket_monitor(backend, "inproc://monitor-pub", ZMQ_EVENT_ALL);
    assert(rc == 0);
    void * backend_mon = zmq_socket(ctx, ZMQ_PAIR);
    assert(backend_mon);
    rc = zmq_connect(backend_mon, "inproc://monitor-pub");
    assert(rc == 0);

    // Receive events on the monitor socket
    int event;
    char *address;
    int value;

    while(1){
        event = get_monitor_event(backend_mon, &value, &address);
        handle_event(event, value, address);
        free(address);
    }
}

static void * task_monitor_frontend(void *arg) {
    // Monitor all events on pub and sub
    int rc = zmq_socket_monitor(frontend, "inproc://monitor-sub", ZMQ_EVENT_ALL);
    assert(rc == 0);

    // Create two sockets for collecting monitor events
    void * frontend_mon = zmq_socket(ctx, ZMQ_PAIR);
    assert(frontend_mon);

    // Connect these to the inproc endpoints so they'll get events
    rc = zmq_connect(frontend_mon, "inproc://monitor-sub");
    assert(rc == 0);

    // Receive events on the monitor socket
    int event;
    char *address;
    int value;

    while(1){
        event = get_monitor_event(frontend_mon, &value, &address);
        handle_event(event, value, address);
        free(address);
    }
}

static void * task_capture(void *arg) {

	printf("Capture/logging task listening on %s\n", sub_str);
    /* Subscriber (RX) */
    void *subscriber = zmq_socket(ctx, ZMQ_SUB);
    if(auth){
        char pub_key[CURVE_KEYLEN] = {0};
        zmq_curve_public(pub_key, sec_key);
        zmq_setsockopt(subscriber, ZMQ_CURVE_SERVERKEY, pub_key, CURVE_KEYLEN);
        zmq_setsockopt(subscriber, ZMQ_CURVE_PUBLICKEY, pub_key, CURVE_KEYLEN);
        zmq_setsockopt(subscriber, ZMQ_CURVE_SECRETKEY, sec_key, CURVE_KEYLEN);
    }
    assert(zmq_connect(subscriber, pub_str) == 0);
    assert(zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0) == 0);

    /* Allocated 'raw' CSP packet */
    csp_packet_t * packet = malloc(CSP_ZMQ_MTU + 16);
    assert(packet != NULL);

    if (logfile_name) {
    	logfile = fopen(logfile_name, "a+");
    	if (logfile == NULL) {
    		printf("Unable to open logfile %s\n", logfile_name);
    		exit(-1);
    	}
    }

    while (1) {
    	zmq_msg_t msg;
        zmq_msg_init(&msg);

        /* Receive data */
        if (zmq_msg_recv(&msg, subscriber, 0) < 0) {
            zmq_msg_close(&msg);
            printf("ZMQ: %s\n", zmq_strerror(zmq_errno()));
            continue;
        }

        int datalen = zmq_msg_size(&msg);
        if (datalen < 5) {
            printf("ZMQ: Too short datalen: %u\n", datalen);
            while(zmq_msg_recv(&msg, subscriber, ZMQ_NOBLOCK) > 0)
                zmq_msg_close(&msg);
            continue;
        }

        /* Copy to packet */
        csp_id_setup_rx(packet);
        memcpy(packet->frame_begin, zmq_msg_data(&msg), datalen);
        packet->frame_length = datalen;

        /* Parse header */
        csp_id_strip(packet);


        /* Print header data */
        printf("Packet: Src %u, Dst %u, Dport %u, Sport %u, Pri %u, Flags 0x%02X, Size %"PRIu16"\n",
                       packet->id.src, packet->id.dst, packet->id.dport,
                       packet->id.sport, packet->id.pri, packet->id.flags, packet->length);

        if (logfile) {
        	const char * delimiter = "--------\n";
        	fwrite(delimiter, sizeof(delimiter), 1, logfile);
        	fwrite(packet->frame_begin, packet->frame_length, 1, logfile);
        	fflush(logfile);
        }

        zmq_msg_close(&msg);
    }
}


int main(int argc, char ** argv) {

	csp_conf.version = 2;

    int opt;
    while ((opt = getopt(argc, argv, shortopts)) != -1) {
        switch (opt) {
            case 'd':
                debug = atoi(optarg);
                break;
            case 'v':
            	csp_conf.version = atoi(optarg);
            	break;
            case 's':
            	sub_str = optarg;
            	break;
            case 'p':
            	pub_str = optarg;
            	break;
            case 'f':
            	logfile_name = optarg;
            	break;
            case 'a':
                auth = 1;
                break;
            case 'g':{
                char public_key[CURVE_KEYLEN], secret_key[CURVE_KEYLEN];
                zmq_curve_keypair(public_key, secret_key);
                printf("Secret key: %s\n", secret_key);
                return 0;
            }
#ifdef ZMQ_PROXY_LOSSY
            case 'L':
            	loss_prob = atof(optarg);
                enable_lossy = 1;
                break;
            case 'C':
            	corr_prob = atof(optarg);
                enable_lossy = 1;
                break;
            case 'D':
            	delay_prob = atof(optarg);
                enable_lossy = 1;
                break;
            case 'T':
            	delay_ms = atoi(optarg);
                break;
            case 'S':
            	seed = atoi(optarg);
                break;
            case 'N':
            	hdx_node = atoi(optarg);
                enable_lossy = 1;
                break;
            case 'U':
            	hdx_powerup_delay = atoi(optarg);
                break;
            case 'O':
            	hdx_powerdown_delay = atoi(optarg);
                break;
#endif
            default:
                printf("Usage:\n"
                       " -d DEBUG_LVL\t1 = connections, 2 = packets, 3 = both\n"
                	   " -v VERSION\tcsp version: (default = 2)\n"
                	   " -s SUB_STR\tsubscriber port: (default = tcp://0.0.0.0:6000)\n"
                	   " -p PUB_STR\tpublisher  port: (default = tcp://0.0.0.0:7000)\n"
                	   " -f LOGFILE\tLog to this file\n"
                	   " -a AUTH\tEnable authentication and encryption\n"
                	   " -g GEN \tGenerate keypair\n"
#ifdef ZMQ_PROXY_LOSSY
                	   " -L LOSS \tProxy with a packet loss probability ex. 0.1 == 10%%\n"
                	   " -C CORR \tProxy with a packet corruption probability ex. 0.1 == 10%%\n"
                	   " -D DELAY \tProxy with a packet delays probability ex. 0.1 == 10%%\n"
                	   " -T TIME \tSet delay time ex. 100ms\n"
                	   " -S SEED \tSeed random number gen else time is used ex 5432542\n"
                	   " -N NODE \tSelect node as half duplex\n"
                	   " -U PWRUP \tTX power up time ms\n"
                	   " -O PWRDWN \tTX power down time ms\n"
#endif
                		);
                exit(1);
                break;
        }
    }

    ctx = zmq_ctx_new();
    assert(ctx);

    frontend = zmq_socket(ctx, ZMQ_XSUB);
    assert(frontend);
    backend = zmq_socket(ctx, ZMQ_XPUB);
    assert(backend);

    if(auth){

        const char *home_dir = getenv("HOME");
        char *file_name = "/zmqauth.cfg";
        char file_path[128] = {0};
        if (home_dir == NULL) {
            printf("HOME environment variable is not set.\n");
            return 1;
        }
        strcpy(file_path, home_dir);
        strcat(file_path, file_name);

        /* Get server secret key from config file */
        FILE *file = fopen(file_path, "r");
        if(file == NULL){
            printf("Could not open config %s\n", file_path);
            return 1;
        }

        if (fgets(sec_key, sizeof(sec_key), file) == NULL) {
            printf("Failed to read secret key from file.\n");
            fclose(file);
            return 1;
        }
        fclose(file);

        int as_server = 1;
        zmq_setsockopt(frontend, ZMQ_CURVE_SERVER, &as_server, sizeof(int));
        zmq_setsockopt(frontend, ZMQ_CURVE_SECRETKEY, sec_key, CURVE_KEYLEN);

        zmq_setsockopt(backend, ZMQ_CURVE_SERVER, &as_server, sizeof(int));
        zmq_setsockopt(backend, ZMQ_CURVE_SECRETKEY, sec_key, CURVE_KEYLEN);
    }

    assert(zmq_bind (frontend, sub_str) == 0);
    printf("Subscriber task listening on %s\n", sub_str);
    assert(zmq_bind(backend, pub_str) == 0);
    printf("Publisher task listening on %s\n", pub_str);

    if(debug & 2){
        pthread_t capworker;
        pthread_create(&capworker, NULL, task_capture, NULL);
    }

    if(debug & 1){
        pthread_t monfworker;
        pthread_create(&monfworker, NULL, task_monitor_frontend, NULL);
        pthread_t monbworker;
        pthread_create(&monbworker, NULL, task_monitor_backend, NULL);
    }

#ifdef ZMQ_PROXY_LOSSY
    if(enable_lossy){
        zmq_proxy_lossy();
    }
#endif

    zmq_proxy(frontend, backend, NULL);

    printf("Closing ZMQproxy");
    zmq_ctx_destroy(ctx);

    return 0;
}

#ifdef ZMQ_PROXY_LOSSY
typedef struct {
    zmq_msg_t msg;
    uint32_t timestamp_tx;
    int hdx;
} delayed_msg_t;

static csp_queue_handle_t delay_handle;

static void * task_delay_send(void *arg) {
    while(1){
front:
        int count = csp_queue_size(delay_handle);
        for(int i = 0; i < count; i++){
            delayed_msg_t dmsg; 
            csp_queue_dequeue(delay_handle, &dmsg, 0);

            if(csp_get_ms() > dmsg.timestamp_tx) {
                zmq_msg_send(&dmsg.msg, backend, 0);
                printf("Delayed packet send\n");
                zmq_msg_close(&dmsg.msg);
                if(dmsg.hdx){
                    hdx_last_tx_ms = csp_get_ms();
                    hdx_powerup_start_ms = 0;
                }
                goto front;
            } else {
                csp_queue_enqueue(delay_handle, &dmsg, 0);
            }
        }
        usleep(100);
    }
    return NULL;
}

void zmq_proxy_lossy() {

    delay_handle = csp_queue_create_static(1024, sizeof(delayed_msg_t), NULL, NULL);
    printf("WARNING PACKET LOSS/CORRUPTION ON \n%.2f%% LOSS \n%.2f%% CORRUPTION\n", loss_prob * 100, corr_prob * 100);
    printf("%.2f%% DELAYED BY %dms\n", delay_prob * 100, delay_ms);
    if (seed) {
        srand(seed);
    } else {
        srand(time(NULL));
    }
    zmq_pollitem_t items[] = {
        { frontend, 0, ZMQ_POLLIN, 0 },
        { backend, 0, ZMQ_POLLIN, 0 }
    };
    pthread_t delayworker;
    pthread_create(&delayworker, NULL, task_delay_send, NULL);

    csp_packet_t * packet = malloc(CSP_ZMQ_MTU + 16);
    assert(packet != NULL);

    while (1) {
        zmq_poll(items, 2, -1);
        if (items[0].revents & ZMQ_POLLIN) {
            zmq_msg_t msg;
            zmq_msg_init(&msg);
            zmq_msg_recv(&msg, frontend, 0);

            /* Simulate half duplex on target node */
            if(hdx_node){
                int datalen = zmq_msg_size(&msg);

                /* Copy to packet */
                csp_id_setup_rx(packet);
                memcpy(packet->frame_begin, zmq_msg_data(&msg), datalen);
                packet->frame_length = datalen;

                /* Parse header */
                csp_id_strip(packet);

                if(packet->id.src == hdx_node && csp_get_ms() > hdx_last_tx_ms + hdx_powerdown_delay){
                    if(!hdx_powerup_start_ms){
                        hdx_powerup_start_ms = csp_get_ms();
                    }
                    delayed_msg_t delayed_msg = {0};
                    delayed_msg.hdx = 1;
                    zmq_msg_init(&delayed_msg.msg);
                    zmq_msg_copy(&delayed_msg.msg, &msg);
                    printf("HDX: TX Delayed packet\n");
                    delayed_msg.timestamp_tx = hdx_powerup_start_ms + hdx_powerup_delay;
                    csp_queue_enqueue(delay_handle, &delayed_msg, 0);
                    zmq_msg_close(&msg);
                    continue;
                }

                if(packet->id.src == hdx_node){
                    hdx_last_tx_ms = csp_get_ms();
                }

                if(packet->id.dst == hdx_node && hdx_last_tx_ms + hdx_powerdown_delay > csp_get_ms()){
                    printf("HDX: RX packet drop\n");
                    zmq_msg_close(&msg);
                    continue;
                }
            }

            /* Simulate corrupted packet */
            if ((double)rand() / RAND_MAX < corr_prob) {
                printf("Packet corrupted\n");
                unsigned int size = zmq_msg_size(&msg);
                if (size > 0) {
                    unsigned char *data = zmq_msg_data(&msg);
                    unsigned int rand_byte = rand() % size;
                    int rand_bit = rand() % 8;
                    data[rand_byte] ^= (1 << rand_bit);
                }
            }

            /* Simulate packet loss */
            if ((double)rand() / RAND_MAX > loss_prob) {
                /* Simulate packet delay */
                if ((double)rand() / RAND_MAX < delay_prob) {
                    delayed_msg_t delayed_msg = {0};
                    zmq_msg_init(&delayed_msg.msg);
                    zmq_msg_copy(&delayed_msg.msg, &msg);
                    printf("Delayed packet\n");
                    delayed_msg.timestamp_tx = csp_get_ms() + delay_ms;
                    csp_queue_enqueue(delay_handle, &delayed_msg, 0);
                    zmq_msg_close(&msg);
                    continue;
                }
                zmq_msg_send(&msg, backend, 0);
            } else {
                printf("Packet dropped\n");
            }
            zmq_msg_close(&msg);
        }
        if (items[1].revents & ZMQ_POLLIN) {
            zmq_msg_t msg;
            zmq_msg_init(&msg);
            zmq_msg_recv(&msg, backend, 0);
            zmq_msg_send(&msg, frontend, 0);
            zmq_msg_close(&msg);
        }
    }
}
#endif