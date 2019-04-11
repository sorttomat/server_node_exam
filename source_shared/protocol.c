#include "protocol.h"
#include <string.h>

void construct_header(char *buffer, struct node node_to_send) {
    memcpy(&(buffer[0]), &(node_to_send.own_address), sizeof(int));
    memcpy(&(buffer[4]), &(node_to_send.number_of_edges), sizeof(int));
}

void deconstruct_header(char *buffer, struct node *node_to_receive) {
    memcpy(&(node_to_receive->own_address), &(buffer[0]), sizeof(int));
    memcpy(&(node_to_receive->number_of_edges), &(buffer[4]), sizeof(int));
}

int receive_message(int client_socket, void *buf, size_t total_bytes_to_receive) {
    char *char_buf;
    size_t received_bytes_this_round = 0;
    size_t total_bytes_received = 0;

    char_buf = buf;
    while (total_bytes_received < total_bytes_to_receive) {
        printf("Receiving message_success\n");
        received_bytes_this_round = recv(client_socket, char_buf + total_bytes_received, total_bytes_to_receive-total_bytes_received, 0);
        printf("This round: %zu\n", received_bytes_this_round);
        if (received_bytes_this_round == -1) {
            printf("Something wring with recv\n");
            return -1;
        }
        if (received_bytes_this_round == 0) {
            printf("Nothing more to receive\n");
            break;
        }
        total_bytes_received += received_bytes_this_round;
        printf("%zu ? %zu\n", total_bytes_received, total_bytes_to_receive);

    }
    return total_bytes_received;
}

int send_message(int client_socket, void *buf, size_t total_bytes_to_send) {
    char *char_buf;
    size_t received_bytes_this_round = 0;
    size_t total_bytes_sent = 0;

    char_buf = buf;
    while (total_bytes_sent < total_bytes_to_send) {
        received_bytes_this_round = send(client_socket, char_buf + total_bytes_sent, total_bytes_to_send-total_bytes_sent, 0);
        printf("This round: %zu\n", received_bytes_this_round);
        if (received_bytes_this_round == -1) {
            printf("Something wring with send\n");
            return EXIT_FAILURE;
        }
        if (received_bytes_this_round == 0) {
            printf("Nothing more to send\n");
            break;
        }
        total_bytes_sent += received_bytes_this_round;
        printf("%zu ? %zu\n", total_bytes_sent, total_bytes_to_send);

    }
    return total_bytes_sent;
}