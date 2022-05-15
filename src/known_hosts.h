#pragma once

void known_hosts_add(int host, char * name);
int known_hosts_get_name(int find_host, char * name, int buflen);
int known_hosts_get_node(char * find_name);