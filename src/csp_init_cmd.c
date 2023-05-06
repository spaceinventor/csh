#include <stdio.h>
#include <stdlib.h>

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
#include "csp_if_cortex.h"
#include <csp/drivers/can_socketcan.h>
#include <csp/drivers/usart.h>
#include <csp/csp_rtable.h>

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
	    return SLASH_EINVAL;
    }

	static struct utsname info;
	uname(&info);

    if (hostname == NULL)
        hostname = info.nodename;

    if (model == NULL)
        model = info.version;

    if (revision == NULL)
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

	return SLASH_SUCCESS;
}

slash_command_sub(csp, init, csp_init_cmd, NULL, "Initialize CSP");


static int csp_ifadd_zmq_cmd(struct slash *slash) {

    static int ifidx = 0;

    char name[10];
    sprintf(name, "ZMQ%u", ifidx++);
    
    int promisc = 0;
    int mask = 8;
    int dfl = 0;

    optparse_t * parser = optparse_new("csp add zmq", "<addr> <server>");
    optparse_add_help(parser);
    optparse_add_set(parser, 'p', "promisc", 1, &promisc, "Promiscuous Mode");
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

	if (++argi >= slash->argc) {
		printf("missing parameter server\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char * server = slash->argv[argi];

    csp_iface_t * iface;
    csp_zmqhub_init_filter2((const char *) name, server, addr, mask, promisc, &iface);
    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

	return SLASH_SUCCESS;
}

slash_command_subsub(csp, add, zmq, csp_ifadd_zmq_cmd, NULL, "Add a new ZMQ interface");

static int csp_ifadd_kiss_cmd(struct slash *slash) {

    static int ifidx = 0;

    char name[10];
    sprintf(name, "KISS%u", ifidx++);
    
    int promisc = 0;
    int mask = 8;
    int dfl = 0;
    int baud = 1000000;
    char * device = "ttyUSB0";

    optparse_t * parser = optparse_new("csp add kiss", "<addr>");
    optparse_add_help(parser);
    optparse_add_set(parser, 'p', "promisc", 1, &promisc, "Promiscuous Mode");
    optparse_add_int(parser, 'm', "mask", "NUM", 0, &mask, "Netmask (defaults to 8)");
    optparse_add_int(parser, 'b', "baud", "NUM", 0, &baud, "Baudrate");
    optparse_add_string(parser, 'u', "uart", "STR", &device, "UART device name (defaults to ttyUSB0)");
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

    csp_usart_conf_t conf = {
        .device = device,
        .baudrate = baud,
        .databits = 8,
        .stopbits = 1,
        .paritysetting = 0,
        .checkparity = 0
    };

    csp_iface_t * iface;
    
    int error = csp_usart_open_and_add_kiss_interface(&conf, name, &iface);
    if (error != CSP_ERR_NONE) {
        printf("Failed to add kiss interface\n");
        return SLASH_EINVAL;
    }

    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

	return SLASH_SUCCESS;
}

slash_command_subsub(csp, add, kiss, csp_ifadd_kiss_cmd, NULL, "Add a new KISS/UART interface");

#if (CSP_HAVE_LIBSOCKETCAN)

static int csp_ifadd_can_cmd(struct slash *slash) {

    static int ifidx = 0;

    char name[10];
    sprintf(name, "CAN%u", ifidx++);
    
    int promisc = 0;
    int mask = 8;
    int dfl = 0;
    int baud = 1000000;
    char * device = "can0";

    optparse_t * parser = optparse_new("csp add can", "<addr>");
    optparse_add_help(parser);
    optparse_add_set(parser, 'p', "promisc", 1, &promisc, "Promiscous Mode");
    optparse_add_int(parser, 'm', "mask", "NUM", 0, &mask, "Netmask (defaults to 8)");
    optparse_add_int(parser, 'b', "baud", "NUM", 0, &baud, "Baudrate");
    optparse_add_string(parser, 'c', "can", "STR", &device, "CAN device name (defaults to can0)");
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

    csp_iface_t * iface;
    
    int error = csp_can_socketcan_open_and_add_interface(device, name, baud, promisc, &iface);
    if (error != CSP_ERR_NONE) {
        csp_print("failed to add CAN interface [%s], error: %d", device, error);
        return SLASH_EINVAL;
    }

    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

	return SLASH_SUCCESS;
}

slash_command_subsub(csp, add, can, csp_ifadd_can_cmd, NULL, "Add a new CAN interface");

#endif


static int csp_ifadd_udp_cmd(struct slash *slash) {

    static int ifidx = 0;

    char name[10];
    sprintf(name, "UDP%u", ifidx++);
    
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
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("missing parameter addr\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char * endptr;
    unsigned int addr = strtoul(slash->argv[argi], &endptr, 10);

	if (++argi >= slash->argc) {
		printf("missing parameter server\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char * server = slash->argv[argi];

    csp_iface_t * iface;
    iface = malloc(sizeof(csp_iface_t));
    memset(iface, 0, sizeof(csp_iface_t));
    csp_if_udp_conf_t * udp_conf = malloc(sizeof(csp_if_udp_conf_t));
    udp_conf->host = strdup(server);
    udp_conf->lport = listen_port;
    udp_conf->rport = remote_port;
    csp_if_udp_init(iface, udp_conf);

    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

	return SLASH_SUCCESS;
}

slash_command_subsub(csp, add, udp, csp_ifadd_udp_cmd, NULL, "Add a new UDP interface");


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
    int rx_port = 9220;
    int tx_port = 9220;

    optparse_t * parser = optparse_new("csp add cortex", "<addr> <server>");
    optparse_add_help(parser);
    optparse_add_set(parser, 'p', "promisc", 1, &promisc, "Promiscous Mode");
    optparse_add_int(parser, 'm', "mask", "NUM", 0, &mask, "Netmask (defaults to 8)");
    optparse_add_set(parser, 'd', "default", 1, &dfl, "Set as default");
    optparse_add_int(parser, 'r', "rx-port", "NUM", 0, &rx_port, "Port to receive data on");
    optparse_add_int(parser, 't', "tx-port", "NUM", 0, &tx_port, "Port to send data to");

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

	if (++argi >= slash->argc) {
		printf("missing parameter server\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}
    char * server = slash->argv[argi];

    csp_iface_t * iface;
    iface = malloc(sizeof(csp_iface_t));
    memset(iface, 0, sizeof(csp_iface_t));
    csp_if_cortex_conf_t * cortex_conf = malloc(sizeof(csp_if_cortex_conf_t));
    cortex_conf->host = strdup(server);
    cortex_conf->rx_port = rx_port;
    cortex_conf->tx_port = tx_port;

    csp_if_cortex_init(&iface, &cortex_conf);

    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

    return SLASH_SUCCESS;
}

slash_command_subsub(csp, add, cortex, csp_ifadd_cortex_cmd, NULL, "Add a new Cortex socket interface");


static int csp_ifadd_tun_cmd(struct slash *slash) {

    static int ifidx = 0;

    char name[10];
    sprintf(name, "TUN%u", ifidx++);
    
    int promisc = 0;
    int mask = 8;
    int dfl = 0;

    optparse_t * parser = optparse_new("csp add udp", "<ifaddr> <tun src> <tun dst>");
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
    csp_if_tun_conf_t * ifconf = malloc(sizeof(csp_if_tun_conf_t));
    ifconf->tun_dst = tun_dst;
    ifconf->tun_src = tun_src;

    csp_if_tun_init(iface, ifconf);

    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

	return SLASH_SUCCESS;
}

slash_command_subsub(csp, add, tun, csp_ifadd_tun_cmd, NULL, "Add a new TUN interface");

#if CSP_HAVE_RTABLE
static int csp_routeadd_cmd(struct slash *slash) {

    char route[50];

    optparse_t * parser = optparse_new("csp add route", "<addr>/<mask> <ifname>");
    optparse_add_help(parser);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);

    if (argi < 0) {
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
    	return SLASH_SUCCESS;
    } else {
        printf("Error during route add\n");
        return SLASH_EINVAL;
    }
}

slash_command_subsub(csp, add, route, csp_routeadd_cmd, NULL, "Add a new route");
#endif
