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

int MAX_NUM_CLIENTS;
struct client *clients;
#endif