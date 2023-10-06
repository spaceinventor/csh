#include <param/param.h>
#include <param/param_client.h>

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <slash/slash.h>
#include <slash/optparse.h>
#include "param_sniffer.h"

static int adcs_node = 0;
static char *default_ip = "127.0.0.1";

#define Q_HAT_ID 305
#define ORBIT_POS 357

int sockfd;
struct sockaddr_in server_addr;

int vts_running = 0;

static double to_jd(uint64_t ts_s) {
	return 2440587.5 + ((ts_s) / 86400.0);
}

int check_vts(uint16_t node, uint16_t id){
    if(!vts_running)
        return 0;
    if(node != adcs_node)
        return 0;
    if(id == Q_HAT_ID || id == ORBIT_POS)
        return true;
    return 0;
}

static uint64_t last_q_hat_time = 0;
static uint64_t last_pos_time = 0;

void vts_add(double arr[4], uint16_t id, int count, uint64_t time_ms){
    char buf[1000];
    uint64_t timestamp = time_ms / 1000;
    double jd_cnes = to_jd(timestamp) - 2433282.5;
    sprintf(buf, "TIME %f 1\n", jd_cnes);
    //printf("%s", buf);
    if(send(sockfd, buf, strlen(buf), MSG_NOSIGNAL) == -1){
        printf("VTS send failed!\n");
    }

    if(id == Q_HAT_ID && count == 4 && timestamp > last_q_hat_time){
        sprintf(buf, "DATA %f orbit_sim_quat \"%f %f %f %f\"\n", jd_cnes, arr[3], arr[0], arr[1], arr[2]);
        //printf("%s", buf);
        if(send(sockfd, buf, strlen(buf), MSG_NOSIGNAL) == -1){
            printf("VTS send failed!\n");
        }
        last_q_hat_time = timestamp;
    }

    if(id == ORBIT_POS && count == 3 && timestamp > last_pos_time){
        sprintf(buf, "DATA %f orbit_prop_pos \"%f %f %f\"\n", jd_cnes, arr[0]/1000, arr[1]/1000, arr[2]/1000); // convert to km
        //printf("%s", buf);
        if(send(sockfd, buf, strlen(buf), MSG_NOSIGNAL) == -1){
            printf("VTS send failed!\n");
        }
        last_pos_time = timestamp;
    }
}

static int vts_init(struct slash * slash) {
    char * server_ip = NULL;
    int port_num = 8888;

    optparse_t * parser = optparse_new("vts init", "");
    optparse_add_help(parser);
    optparse_add_string(parser, 's', "server-ip", "STRING", &server_ip, "Overwrite default ip 127.0.0.1");
    optparse_add_int(parser, 'p', "server-port", "NUM", 0, &port_num, "Overwrite default port 8888");
    optparse_add_int(parser, 'n', "adcs-node", "NUM", 0, &adcs_node, "Set adcs node");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);

    if (argi < 0) {
        optparse_del(parser);
        return SLASH_EINVAL;
    }

    if(!server_ip){
        server_ip = default_ip;
    }

	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd < 0) {
		printf("Failed to get socket for VTS\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_num);

    if(inet_pton(AF_INET, server_ip, &server_addr.sin_addr)<=0) {
        printf("Invalid address/ Address not supported\n");
		close(sockfd);
        optparse_del(parser);
		return SLASH_EINVAL;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Connection failed\n");
		close(sockfd);
        optparse_del(parser);
		return SLASH_EINVAL;
    }

	const char * init_cmd = "INIT adcs REGULATING\n";
	if (send(sockfd, init_cmd, strlen(init_cmd), MSG_NOSIGNAL) == -1) {
		printf("Failed to send init\n");
		close(sockfd);
        optparse_del(parser);
		return SLASH_EINVAL;
    }

    param_sniffer_init(0, 0);
    printf("Streaming data to VTS at %s:%d\n", server_ip,port_num);
    vts_running = 1;

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_sub(vts, init, vts_init, "", "Push data to VTS");

