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

    static int zmqidx = 0;

    char name[10];
    sprintf(name, "ZMQ%u", zmqidx++);
    
    int promisc = 0;
    int mask = 8;
    int dfl = 0;

    optparse_t * parser = optparse_new("csp add zmq", "<addr> <server>");
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
