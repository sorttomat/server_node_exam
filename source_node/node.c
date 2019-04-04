#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>

int BASEPORT = 0;

int create_and_connect_socket() {
    struct sockaddr_in server_addr;
    char buffer[255];

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

    ssize_t bytes = recv(client_socket, buffer, sizeof(char) * 255, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
    }

    printf("%s\n", buffer);

    char send_text[255];
    fgets(send_text, 255, stdin); //For morroskyld
    int number_of_bytes = strlen(send_text);

    bytes = send(client_socket, &number_of_bytes, sizeof(int), 0);
    bytes = send(client_socket, send_text, strlen(send_text), 0);

    bytes = recv(client_socket, buffer, sizeof(char) * 255, 0);
    printf("%s\n", buffer);

    close(client_socket);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    assert(argc == 2);

    BASEPORT = atoi(argv[1]);
    int ret = create_and_connect_socket();

    if (ret == 1) {
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
} 