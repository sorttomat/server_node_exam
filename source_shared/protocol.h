
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

char RESPONSE_SUCCESS[] = "Thank you for connecting!\n";
char RESPONSE_FULL[] = "Client list is full :(\n";
char ALL_CLIENTS_CONNECTED[] = "All clients connected! Server disconnecting in 3... 2... 1...\n";


void construct_header(char *buffer, struct node node_to_send);
void deconstruct_header(char *buffer, struct node *node_to_receive);
int receive_message(int client_socket, void *buf, size_t total_bytes_to_receive);
int send_message(int client_socket, void *buf, size_t total_bytes_to_send);

#endif