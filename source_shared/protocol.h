
#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>

struct edge {
    int from_address;
    int to_address;
    int weight;
};

struct node {
    int own_address;
    int client_socket;
    struct edge *edges;
    int number_of_edges;
};

struct header {
    int own_address;
    int number_of_edges;
};

struct table {
    int to_address;
    int first_client_on_route;
};

char *RESPONSE_SUCCESS;
char *RESPONSE_FULL;
char *ALL_CLIENTS_CONNECTED;


void construct_header(char *buffer, struct node node_to_send);
ssize_t receive_message(int client_socket, void *buf, ssize_t total_bytes_to_receive);
ssize_t send_message(int client_socket, void *buf, ssize_t total_bytes_to_send);

#endif
