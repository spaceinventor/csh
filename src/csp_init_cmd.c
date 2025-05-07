#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <csp/csp.h>

#include <pthread.h>
#include <param/param_server.h>
#include <vmem/vmem_server.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <sys/utsname.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include <csp/interfaces/csp_if_can.h>
#include <csp/interfaces/csp_if_lo.h>
#include <csp/interfaces/csp_if_tun.h>
#include <csp/interfaces/csp_if_udp.h>
#include <csp/interfaces/csp_if_eth.h>
#include <csp/drivers/can_socketcan.h>
#include <csp/drivers/eth_linux.h>
#include <csp/drivers/usart.h>
#include <csp/csp_rtable.h>
#include <ifaddrs.h>

#define CURVE_KEYLEN 41

void * router_task(void * param) {
	while(1) {
		csp_route_work();
	}
}

void * vmem_server_task(void * param) {
	vmem_server_loop(param);
	return NULL;
}

static int csp_init_cmd(struct slash *slash) {

    char * hostname = NULL;
    char * model = NULL;
    char * revision = NULL;
	int version = 2;
	int dedup = 3;

    optparse_t * parser = optparse_new("csp init", "");
    optparse_add_help(parser);
    optparse_add_string(parser, 'n', "host", "HOSTNAME", &hostname, "Hostname (default = linux hostname)");
    optparse_add_string(parser, 'm', "model", "MODELNAME", &model, "Model name (default = linux get domain name)");
    optparse_add_string(parser, 'r', "revision", "REVSION", &revision, "Revision (default = release name)");
    optparse_add_int(parser, 'v', "version", "NUM", 0, &version, "CSP version (default = 2)");
    optparse_add_int(parser, 'd', "dedup", "NUM", 0, &dedup, "CSP dedup 0=off 1=forward 2=incoming 3=all (default)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	static struct utsname info;
	uname(&info);

    if (hostname){
        strncpy(info.nodename, hostname, _UTSNAME_NODENAME_LENGTH - 1);
    }
    if (model){
        strncpy(info.version, model, _UTSNAME_VERSION_LENGTH - 1);
    }
    if (revision){
        strncpy(info.release, revision, _UTSNAME_RELEASE_LENGTH - 1);
    }

    hostname = info.nodename;
    model = info.version;
    revision = info.release;

    printf("  Version %d\n", version);
    printf("  Hostname: %s\n", hostname);
    printf("  Model: %s\n", model);
    printf("  Revision: %s\n", revision);
    printf("  Deduplication: %d\n", dedup);

    csp_conf.hostname = hostname;
	csp_conf.model = model;
	csp_conf.revision = revision;
	csp_conf.version = version;
	csp_conf.dedup = dedup;
	csp_init();

    csp_bind_callback(csp_service_handler, CSP_ANY);
	csp_bind_callback(param_serve, PARAM_PORT_SERVER);

	static pthread_t router_handle;
	pthread_create(&router_handle, NULL, &router_task, NULL);

	static pthread_t vmem_server_handle;
	pthread_create(&vmem_server_handle, NULL, &vmem_server_task, NULL);

    csp_iflist_check_dfl();

	csp_rdp_set_opt(3, 10000, 5000, 1, 2000, 2);
	//csp_rdp_set_opt(5, 10000, 5000, 1, 2000, 4);
	//csp_rdp_set_opt(10, 10000, 5000, 1, 2000, 8);
	//csp_rdp_set_opt(25, 10000, 5000, 1, 2000, 20);
	//csp_rdp_set_opt(40, 3000, 1000, 1, 250, 35);

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command_sub(csp, init, csp_init_cmd, NULL, "Initialize CSP");


static int csp_ifadd_zmq_cmd(struct slash *slash) {

    static int ifidx = 0;

    char name[CSP_IFLIST_NAME_MAX+1] = {0};
    snprintf(name, CSP_IFLIST_NAME_MAX, "ZMQ%u", ifidx++);
    
    int promisc = 0;
    int mask = 8;
    int dfl = 0;
    char * key_file = NULL;
    char * sec_key = NULL;
    unsigned int subport = 0;
    unsigned int pubport = 0;

    optparse_t * parser = optparse_new("csp add zmq", "<addr> <server>");
    optparse_add_help(parser);
    optparse_add_set(parser, 'p', "promisc", 1, &promisc, "Promiscuous Mode");
    optparse_add_int(parser, 'm', "mask", "NUM", 0, &mask, "Netmask (defaults to 8)");
    optparse_add_set(parser, 'd', "default", 1, &dfl, "Set as default");
    optparse_add_string(parser, 'a', "auth_file", "STR", &key_file, "file containing private key for zmqproxy (default: None)");
    optparse_add_unsigned(parser, 'S', "subport", "NUM", 0, &subport, "Subscriber port of zmqproxy (default: 6000, encrypted: 6001)");
    optparse_add_unsigned(parser, 'P', "pubport", "NUM", 0, &pubport, "Publisher port of zmqproxy (default: 7000, encrypted: 7001)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("missing parameter addr\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char * endptr = NULL;
    const unsigned int addr = strtoul(slash->argv[argi], &endptr, 10);

    if (*endptr != '\0') {
        fprintf(stderr, "Addr argument '%s' is not an integer\n", slash->argv[argi]);
        optparse_del(parser);
		return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("missing parameter server\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char * server = slash->argv[argi];

    if (subport == 0) {
        subport = CSP_ZMQPROXY_SUBSCRIBE_PORT + ((key_file == NULL) ? 0 : 1);
    }

    if (pubport == 0) {
        pubport = CSP_ZMQPROXY_PUBLISH_PORT + ((key_file == NULL) ? 0 : 1);
    }

    if(key_file) {

        char key_file_local[256];
        if (key_file[0] == '~') {
            strcpy(key_file_local, getenv("HOME"));
            strcpy(&key_file_local[strlen(key_file_local)], &key_file[1]);
        }
        else {
            strcpy(key_file_local, key_file);
        }

        FILE *file = fopen(key_file_local, "r");
        if(file == NULL){
            printf("Could not open config %s\n", key_file_local);
            optparse_del(parser);
            return SLASH_EINVAL;
        }

        sec_key = malloc(CURVE_KEYLEN * sizeof(char));
        if (sec_key == NULL) {
            printf("Failed to allocate memory for secret key.\n");
            fclose(file);
            optparse_del(parser);
            return SLASH_EINVAL;
        }

        if (fgets(sec_key, CURVE_KEYLEN, file) == NULL) {
            printf("Failed to read secret key from file.\n");
            free(sec_key);
            fclose(file);
            optparse_del(parser);
            return SLASH_EINVAL;
        }
        /* We are most often saved from newlines, by only reading out CURVE_KEYLEN.
            But we still attempt to strip them, in case someone decides to use a short key. */
        char * const newline = strchr(sec_key, '\n');
        if (newline) {
            *newline = '\0';
        }
        fclose(file);
    }

    csp_iface_t * iface;
    int error = csp_zmqhub_init_filter2((const char *) name, server, addr, mask, promisc, &iface, sec_key, subport, pubport);
    if (error != CSP_ERR_NONE) {
        csp_print("Failed to add zmq interface [%s], error: %d\n", server, error);
        optparse_del(parser);
        return SLASH_EINVAL;
    }
    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

    if (sec_key != NULL) {
        free(sec_key);
    }
    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command_subsub(csp, add, zmq, csp_ifadd_zmq_cmd, NULL, "Add a new ZMQ interface");

static int csp_ifadd_kiss_cmd(struct slash *slash) {

    static int ifidx = 0;

    char name[CSP_IFLIST_NAME_MAX+1] = {0};
    snprintf(name,CSP_IFLIST_NAME_MAX, "KISS%u", ifidx++);
    
    int mask = 8;
    int dfl = 0;
    int baud = 1000000;
    char * device = "/dev/ttyUSB0";

    optparse_t * parser = optparse_new("csp add kiss", "<addr>");
    optparse_add_help(parser);
    optparse_add_int(parser, 'm', "mask", "NUM", 0, &mask, "Netmask (defaults to 8)");
    optparse_add_int(parser, 'b', "baud", "NUM", 0, &baud, "Baudrate (defaults to 1000000)");
    optparse_add_string(parser, 'u', "uart", "STR", &device, "UART device name (defaults to /dev/ttyUSB0)");
    optparse_add_set(parser, 'd', "default", 1, &dfl, "Set as default");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("missing parameter addr\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char * endptr = NULL;
    const unsigned int addr = strtoul(slash->argv[argi], &endptr, 10);

    if (*endptr != '\0') {
        fprintf(stderr, "Addr argument '%s' is not an integer\n", slash->argv[argi]);
        optparse_del(parser);
		return SLASH_EINVAL;
    }

    csp_usart_conf_t conf = {
        .device = device,
        .baudrate = baud,
        .databits = 8,
        .stopbits = 1,
        .paritysetting = 0
    };

    csp_iface_t * iface;
    
    int error = csp_usart_open_and_add_kiss_interface(&conf, name, addr, &iface);
    if (error != CSP_ERR_NONE) {
        csp_print("Failed to add kiss interface [%s], error: %d\n", device, error);
        optparse_del(parser);
        return SLASH_EINVAL;
    }

    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command_subsub(csp, add, kiss, csp_ifadd_kiss_cmd, NULL, "Add a new KISS/UART interface");

#if (CSP_HAVE_LIBSOCKETCAN)

static int csp_ifadd_can_cmd(struct slash *slash) {

    static int ifidx = 0;

    char name[CSP_IFLIST_NAME_MAX+1] = {0};
    snprintf(name, CSP_IFLIST_NAME_MAX, "CAN%u", ifidx++);
    
    int promisc = 0;
    int mask = 8;
    int dfl = 0;
    int baud = 1000000;
    char * device = "can0";

    optparse_t * parser = optparse_new("csp add can", "<addr>");
    optparse_add_help(parser);
    optparse_add_set(parser, 'p', "promisc", 1, &promisc, "Promiscous Mode");
    optparse_add_int(parser, 'm', "mask", "NUM", 0, &mask, "Netmask (defaults to 8)");
    optparse_add_int(parser, 'b', "baud", "NUM", 0, &baud, "Baudrate (defaults to 1000000)");
    optparse_add_string(parser, 'c', "can", "STR", &device, "CAN device name (defaults to can0)");
    optparse_add_set(parser, 'd', "default", 1, &dfl, "Set as default");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("missing parameter addr\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char * endptr = NULL;
    const unsigned int addr = strtoul(slash->argv[argi], &endptr, 10);

    if (*endptr != '\0') {
        fprintf(stderr, "Addr argument '%s' is not an integer\n", slash->argv[argi]);
        optparse_del(parser);
		return SLASH_EINVAL;
    }

    csp_iface_t * iface;
    
    int error = csp_can_socketcan_open_and_add_interface(device, name, addr, baud, promisc, &iface);
    if (error != CSP_ERR_NONE) {
        csp_print("failed to add CAN interface [%s], error: %d\n", device, error);
        optparse_del(parser);
        return SLASH_EINVAL;
    }

    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command_subsub(csp, add, can, csp_ifadd_can_cmd, NULL, "Add a new CAN interface");

#endif

static void eth_select_interface(const char ** device) {

    static char selected[20];
    selected[0] = 0;

    // Create link of interface adresses
    struct ifaddrs *addresses;
    if (getifaddrs(&addresses) == -1)  {
        printf("eth_select_interface: getifaddrs call failed\n");
    } else {
        // Search for match
        struct ifaddrs * address = addresses;

        for( ; address && (selected[0] == 0); address = address->ifa_next) {
            if (address->ifa_addr && strcmp("lo", address->ifa_name) != 0) {
                if (strncmp(*device, address->ifa_name, strlen(*device)) == 0) {
                    strncpy(selected, address->ifa_name, sizeof(selected)-1);  // -1 to fit NULL byte
                }
            }
        }
        freeifaddrs(addresses);
    }

    if (selected[0] == 0) {
        printf("  Device prefix '%s' not found.\n", *device);
    }
    *device = selected;
}

static int csp_ifadd_eth_cmd(struct slash *slash) {

    static int ifidx = 0;
    char name[CSP_IFLIST_NAME_MAX+1] = {0};
    snprintf(name, CSP_IFLIST_NAME_MAX, "ETH%u", ifidx++);
    const char * device = "e";
   
    int promisc = 0;
    int mask = 8;
    int dfl = 0;
    int mtu = 1200;

    optparse_t * parser = optparse_new("csp add eth", "<addr>");
    optparse_add_help(parser);
    optparse_add_string(parser, 'e', "device", "STR", (char **)&device, "Ethernet device name or name prefix (defaults to enp)");
    optparse_add_set(parser, 'p', "promisc", 1, &promisc, "Promiscous Mode");
    optparse_add_int(parser, 'm', "mask", "NUM", 0, &mask, "Netmask (defaults to 8)");
    optparse_add_set(parser, 'd', "default", 1, &dfl, "Set as default");
    optparse_add_int(parser, 'b', "mtu", "NUM", 0, &mtu, "MTU in bytes (default 1200)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("missing parameter addr\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char * endptr = NULL;
    const unsigned int addr = strtoul(slash->argv[argi], &endptr, 10);

    if (*endptr != '\0') {
        fprintf(stderr, "Addr argument '%s' is not an integer\n", slash->argv[argi]);
        optparse_del(parser);
		return SLASH_EINVAL;
    }

    eth_select_interface(&device);
    if (strlen(device) == 0) {
        optparse_del(parser);
		return SLASH_EINVAL;
    }

    csp_iface_t * iface = NULL;

    // const char * device, const char * ifname, int mtu, unsigned int node_id, csp_iface_t ** iface, bool promisc
    int error = csp_eth_init(device, name, mtu, addr, promisc == 1, &iface);
    if (error != CSP_ERR_NONE) {
        csp_print("Failed to add ethernet interface [%s], error: %d\n", device, error);
        optparse_del(parser);
        return SLASH_EINVAL;
    }

    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

    optparse_del(parser);
    return SLASH_SUCCESS;
}

slash_command_subsub(csp, add, eth, csp_ifadd_eth_cmd, NULL, "Add a new Ethernet interface");

static int csp_ifadd_udp_cmd(struct slash *slash) {

    static int ifidx = 0;

    char name[CSP_IFLIST_NAME_MAX+1] = {0};
    snprintf(name, CSP_IFLIST_NAME_MAX, "UDP%u", ifidx++);
    
    int promisc = 0;
    int mask = 8;
    int dfl = 0;
    int listen_port = 9220;
    int remote_port = 9220;

    optparse_t * parser = optparse_new("csp add udp", "<addr> <server>");
    optparse_add_help(parser);
    optparse_add_set(parser, 'p', "promisc", 1, &promisc, "Promiscous Mode");
    optparse_add_int(parser, 'm', "mask", "NUM", 0, &mask, "Netmask (defaults to 8)");
    optparse_add_int(parser, 'l', "listen-port", "NUM", 0, &listen_port, "Port to listen on");
    optparse_add_int(parser, 'r', "remote-port", "NUM", 0, &remote_port, "Port to send to");
    optparse_add_set(parser, 'd', "default", 1, &dfl, "Set as default");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("missing parameter addr\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char * endptr = NULL;
    const unsigned int addr = strtoul(slash->argv[argi], &endptr, 10);

    if (*endptr != '\0') {
        fprintf(stderr, "Addr argument '%s' is not an integer\n", slash->argv[argi]);
        optparse_del(parser);
		return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("missing parameter server\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char * server = slash->argv[argi];

    csp_iface_t * iface;
    iface = malloc(sizeof(csp_iface_t));
    if(!iface) {
        fprintf(stderr, "Cannot allocate memory\n");
        optparse_del(parser);
		return SLASH_EINVAL;
    }
    memset(iface, 0, sizeof(csp_iface_t));
    csp_if_udp_conf_t * udp_conf = malloc(sizeof(csp_if_udp_conf_t));
    if(!udp_conf) {
        fprintf(stderr, "Cannot allocate memory\n");
        free(iface);
        optparse_del(parser);
		return SLASH_EINVAL;
    }
    udp_conf->host = strdup(server);
    udp_conf->lport = listen_port;
    udp_conf->rport = remote_port;
    csp_if_udp_init(iface, udp_conf);

    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command_subsub(csp, add, udp, csp_ifadd_udp_cmd, NULL, "Add a new UDP interface");

static int csp_ifadd_tun_cmd(struct slash *slash) {

    static int ifidx = 0;

    char name[CSP_IFLIST_NAME_MAX+1] = {0};
    snprintf(name, CSP_IFLIST_NAME_MAX, "TUN%u", ifidx++);
    
    int promisc = 0;
    int mask = 8;
    int dfl = 0;

    optparse_t * parser = optparse_new("csp add tun", "<ifaddr> <tun src> <tun dst>");
    optparse_add_help(parser);
    optparse_add_set(parser, 'p', "promisc", 1, &promisc, "Promiscous Mode");
    optparse_add_int(parser, 'm', "mask", "NUM", 0, &mask, "Netmask (defaults to 8)");
    optparse_add_set(parser, 'd', "default", 1, &dfl, "Set as default");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("missing parameter addr\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char * endptr = NULL;
    const unsigned int addr = strtoul(slash->argv[argi], &endptr, 10);

    if (*endptr != '\0') {
        fprintf(stderr, "Addr argument '%s' is not an integer\n", slash->argv[argi]);
        optparse_del(parser);
		return SLASH_EINVAL;
    }

    if (++argi >= slash->argc) {
		printf("missing parameter tun src\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    unsigned int tun_src = strtoul(slash->argv[argi], &endptr, 10);

    if (++argi >= slash->argc) {
		printf("missing parameter tun dst\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    unsigned int tun_dst = strtoul(slash->argv[argi], &endptr, 10);

    csp_iface_t * iface;
    iface = malloc(sizeof(csp_iface_t));
    if(!iface) {
        fprintf(stderr, "Cannot allocate memory\n");
        optparse_del(parser);
		return SLASH_EINVAL;
    }
    csp_if_tun_conf_t * ifconf = malloc(sizeof(csp_if_tun_conf_t));
    if(!ifconf) {
        fprintf(stderr, "Cannot allocate memory\n");
        free(iface);
        optparse_del(parser);
		return SLASH_EINVAL;
    }
    ifconf->tun_dst = tun_dst;
    ifconf->tun_src = tun_src;

    csp_if_tun_init(iface, ifconf);

    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

    optparse_del(parser);
	return SLASH_SUCCESS;
}

slash_command_subsub(csp, add, tun, csp_ifadd_tun_cmd, NULL, "Add a new TUN interface");

#if CSP_USE_RTABLE
static int csp_routeadd_cmd(struct slash *slash) {

    char route[50];

    optparse_t * parser = optparse_new("csp add route", "<addr>/<mask> <ifname>");
    optparse_add_help(parser);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    /* Build string from the two slash input arguments */
	if (++argi >= slash->argc) {
		printf("missing parameter addr/mask\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    strcpy(route, slash->argv[argi]);
    route[strlen(slash->argv[argi])] = ' ';
    route[strlen(slash->argv[argi])+1] = '\0';

    if (++argi >= slash->argc) {
		printf("missing parameter ifname\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

    strcpy(&route[strlen(route)], slash->argv[argi]);

    if (csp_rtable_load(route) == 1) { /* function returns number of routes added */
        printf("Added route %s\n", route);
        optparse_del(parser);
    	return SLASH_SUCCESS;
    } else {
        printf("Error during route add\n");
        optparse_del(parser);
        return SLASH_EINVAL;
    }
}

slash_command_subsub(csp, add, route, csp_routeadd_cmd, NULL, "Add a new route");
#endif
