#include "protocol.h"
#include <string.h>

char *RESPONSE_SUCCESS = "Thank you for connecting!\n";
char *RESPONSE_FULL = "Client list is full :(\n";
char *ALL_CLIENTS_CONNECTED = "All clients connected! Server disconnecting in 3... 2... 1...\n";

void construct_header(char *buffer, struct node node_to_send) {
    memcpy(&(buffer[0]), &(node_to_send.own_address), sizeof(int));
    memcpy(&(buffer[4]), &(node_to_send.number_of_edges), sizeof(int));
}

ssize_t receive_message(int client_socket, void *buf, ssize_t total_bytes_to_receive) {
    char *char_buf;
    ssize_t received_bytes_this_round = 0;
    ssize_t total_bytes_received = 0;

    char_buf = buf;
    while (total_bytes_received < total_bytes_to_receive) {
        received_bytes_this_round = recv(client_socket, char_buf + total_bytes_received, total_bytes_to_receive-total_bytes_received, 0);
        if (received_bytes_this_round == -1) {
            printf("Something wring with recv\n");
            return -1;
        }
        if (received_bytes_this_round == 0) {
            printf("Nothing more to receive\n");
            break;
        }
        total_bytes_received += received_bytes_this_round;
    }
    return total_bytes_received;
}

ssize_t send_message(int client_socket, void *buf, ssize_t total_bytes_to_send) {
    char *char_buf;
    ssize_t received_bytes_this_round = 0;
    ssize_t total_bytes_sent = 0;

    char_buf = buf;
    while (total_bytes_sent < total_bytes_to_send) {
        received_bytes_this_round = send(client_socket, char_buf + total_bytes_sent, total_bytes_to_send-total_bytes_sent, 0);
        if (received_bytes_this_round == -1) {
            printf("Something wring with send\n");
            return EXIT_FAILURE;
        }
        if (received_bytes_this_round == 0) {
            printf("Nothing more to send\n");
            break;
        }
        total_bytes_sent += received_bytes_this_round;
    }
    return total_bytes_sent;
}
