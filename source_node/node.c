#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include "../source_shared/protocol.h"

int BASEPORT = 0;

struct node *NODE;

int create_and_connect_socket() {
    struct sockaddr_in server_addr;

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BASEPORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("Connecting to router ..........\n");
    int ret = connect(client_socket, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
    if (ret == -1) {
        perror("connect");
        return EXIT_FAILURE;
    }

    return client_socket;
}

int send_and_receive(int client_socket) {
    char buffer[255];

    ssize_t bytes = recv(client_socket, buffer, sizeof(char) * 255, 0);
    if (bytes == -1) {
        printf("Something wrong with recv\n");
        return EXIT_FAILURE;
    }
    if (bytes == 0) {
        printf("Server has disconnected\n");
        return EXIT_SUCCESS; //?
    }

    buffer[bytes] = '\0';
    printf("%s\n", buffer);

    char send_text[255];
    fgets(send_text, 255, stdin); //For morroskyld
    int number_of_bytes = strlen(send_text);

    bytes = send(client_socket, &number_of_bytes, sizeof(int), 0);
    bytes = send(client_socket, send_text, strlen(send_text), 0);

    bytes = recv(client_socket, buffer, sizeof(char) * 255, 0);
    if (bytes == -1) {
        return EXIT_FAILURE;
    }
    if (bytes == 0) {
        printf("Server has disconnected\n");
        return EXIT_SUCCESS; //?
    }

    printf("%s\n", buffer);

    close(client_socket);
    return EXIT_SUCCESS;
}

struct edge create_edge(char *info, int size_of_info) {
    struct edge new_edge;
    char buf[size_of_info];
    int size_weight;

    int i = 0;
    while (info[i] != ':') {
        buf[i] = info[i];
        i++;
    }
    buf[i] = '\0';

    new_edge.to_address = atoi(buf);

    size_weight = size_of_info-(i);
    
    char weight[size_weight];
    memcpy(&weight, &(info[i+1]), size_weight);
    
    new_edge.weight = atoi(weight);

    return new_edge;
}

int main(int argc, char *argv[]) {
    if (argc <= 2) {
        printf("Number of arguments are too few!\n");
        exit(EXIT_FAILURE);
    }

    BASEPORT = atoi(argv[1]);
    int client_socket = create_and_connect_socket();

    if (client_socket == EXIT_FAILURE) {
        exit(EXIT_FAILURE);
    }

    NODE = malloc(sizeof(struct node));

    NODE->client_socket = client_socket;
    NODE->own_address = atoi(argv[2]);

    for (int i = 3; i < argc; i++) {
        struct edge new_edge = create_edge(argv[i], strlen(argv[i]));
        new_edge.from_address = NODE->own_address;
        printf("Edge address: %d\nEdge weight: %d\n", new_edge.to_address, new_edge.weight);
    }

    send_and_receive(client_socket);

    return EXIT_SUCCESS;
} 