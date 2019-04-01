#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

int BASEPORT;
int BACKLOG_SIZE = 10;

int create_server_socket() {
    int ret, yes, server_socket = 1;
    struct sockaddr_in server_addr;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket == -1) {
        perror("Could not create socket :(");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET; /* IPv4 address */
    server_addr.sin_port = htons(BASEPORT); /* Listen to port 21400. Encode 21400 using network byte order.*/
    server_addr.sin_addr.s_addr = INADDR_ANY; /* Accept connections from anyone. */
    
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    ret = bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));

    if (ret == 1) {
        perror("Could not bind to address");
        exit(EXIT_FAILURE);
    }

    ret = listen(server_socket, BACKLOG_SIZE);

    if (ret == -1) {
        perror("Could not listen");
        exit(EXIT_FAILURE);
    }

    return server_socket;
}

void run_server(int socket_listener) {
    fd_set fds, read_fds;
    char* buf;
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    int max_size_recv = 1000;
    int recv_bytes;
    
    FD_ZERO(&fds);
    FD_SET(socket_listener, &fds);

    char exit[] = "exit";


    while(1) {
        fds = read_fds;
        printf("Hit men ikke lenger 0\n");

        /*if (select(socket_listener+1, &fds, NULL, NULL, NULL) == -1) {
            perror("select");
            return;
        }*/

        printf("Hit men ikke lenger 1\n");
        FD_SET(socket_listener, &fds);

        if (FD_ISSET(socket_listener, &fds)) {
            printf("Hit men ikke lenger 2\n");

            int client_socket = accept(socket_listener, (struct sockaddr*)&client_addr, &addrlen);

            printf("Client socket: %d\n", client_socket);

            if (client_socket == -1) {
                perror("accept");
                return;
            }

            printf("Press enter to continue\n");


            buf = malloc(sizeof(char) * max_size_recv+1);

            do {
                recv_bytes = recv(client_socket, buf, max_size_recv-1, 0);
                buf[recv_bytes-2] = '\0';

            } while (strcmp(buf, exit) != 0);

        }
    }
}

int main(int argc, char *argv[]) {
    assert(argc == 2);

    BASEPORT = atoi(argv[1]);

    int server_socket = create_server_socket();

    run_server(server_socket);
    return 0;
}