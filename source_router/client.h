#ifndef __CLIENT_H
#define __CLIENT_H

struct client {
    int client_socket;
    char *ip;
    unsigned short port;

    int own_address;
    int number_of_edges;
    struct edge *edges;
};

int max_num_clients;
struct client *clients;
#endif
