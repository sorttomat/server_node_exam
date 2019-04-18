#ifndef __ROUTING_H
#define __ROUTING_H

struct client {
    int client_socket;
    char *ip;
    unsigned short port;

    int own_address;
    int number_of_edges;
    struct edge *edges;
};

struct table {
    int to_address;
    int weight;
    int first_client_on_route;
};

#endif
