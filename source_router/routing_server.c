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
int MAX_NUM_CLIENTS;

char RESPONSE_SUCCESS[] = "Thank you for connecting!\n";
char RESPONSE_FULL[] = "Client list is full :(\n";
struct client *clients;

struct client {
    int fd;
    char *ip;
    unsigned short port;
    char *name;
    int number_of_neighbors;
};

void remove_client(int fd, struct client *clients) {
    int i;

    for (i = 0; i < MAX_NUM_CLIENTS; ++i) {
        if (clients[i].fd == fd) {
            clients[i].fd = -1;
            free(clients[i].ip);
            return;
        }
    }
}

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

int add_client(int socket, struct sockaddr_in *client_addr, socklen_t addrlen, struct client clients[]) {
    char *ip_buffer = malloc(16);
    int i;


    if (!inet_ntop(client_addr->sin_family, &(client_addr->sin_addr), ip_buffer, addrlen)) {
        perror("inet_ntop");
        strcpy(ip_buffer, "N/A");
    }


    for (i = 0; i < MAX_NUM_CLIENTS; ++i) {
        /* We have capacity to handle this client*/
        if (clients[i].fd == -1) {
            clients[i].fd = socket;
            clients[i].ip = ip_buffer;
            clients[i].port = ntohs(client_addr->sin_port);

            send(socket, RESPONSE_SUCCESS, strlen(RESPONSE_SUCCESS), 0);
            return 0;
        }
    }

    /* We don't have capacity to handle this client*/
    send(socket, RESPONSE_FULL, strlen(RESPONSE_FULL), 0);
    free(ip_buffer);
    close(socket);
    return 1;
}

int accept_new_client(int socket_listener, struct client *clients) {
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    int client_socket = accept(socket_listener,
        (struct sockaddr*)&client_addr,
        &addrlen);
    if (client_socket == -1) {
        perror("accept");
        return -1;
    }

    if (add_client(client_socket, &client_addr, addrlen, clients)) {
        return -1;
    }
    return client_socket;
}

char *receive_message(int client_socket, int *bytes_received) {
    int recv_bytes = 0;

    int total_size = 0;
    int remaining_recv = 0;

    recv_bytes = recv(client_socket, &total_size, sizeof(int), 0);
    if (recv_bytes < 0) {
        return NULL;
    }

    printf("Bytes to receive: %d\n", total_size);
    remaining_recv = total_size;

    char *message = malloc(total_size+1);

    while (*bytes_received != total_size) {
        recv_bytes = recv(client_socket, message + *bytes_received, remaining_recv, 0);

        if (recv_bytes < 0) {
            return NULL;
        }

        if (recv_bytes == 0) {
            break;
        }
        *bytes_received += recv_bytes;
        remaining_recv -= recv_bytes;
    }

    return message;
}

int receive_from_client(int client_socket, struct client *clients) {
    int max_size_recv = 1000;
    int recv_bytes = 0;

    char *message;

    message = receive_message(client_socket, &recv_bytes);
    message[recv_bytes] = '\0';

    printf("Number of bytes: %d\n", recv_bytes);
    if (message == NULL) {
        return 1;
    }
    printf("Message: %s\n", message);

    for (int j = 0; j < MAX_NUM_CLIENTS; j++) {
        if (clients[j].fd == client_socket) {
            clients[j].name = message;
        }
    }
    return 0;
}

int go_through_fds(int socket_listener, fd_set *read_fds, fd_set *fds, int *largest_fd, struct client *clients, int *number_of_clients_added) {
        /* Go through all possible file descriptors */
    for (int i = 0; i <= *largest_fd; ++i) {
        if (FD_ISSET(i, read_fds)) {
            if (i == socket_listener) {
                /* Accept new client */
                int client_socket = accept_new_client(socket_listener, clients);

                if (client_socket == -1) {
                    perror("accept or add");
                    continue;
                }
                if (client_socket > *largest_fd) {
                    *largest_fd = client_socket;
                }

                FD_SET(client_socket, fds);
            } 
            else {
                /* Receive from existing client */
                int ret = receive_from_client(i, clients);
                
                if (ret == 0) {
                    (*number_of_clients_added)++;
                    printf("%d\n", *number_of_clients_added);
                    if (*number_of_clients_added == MAX_NUM_CLIENTS) {
                        return 0;
                    }

                } else if (ret == 1) {
                    FD_CLR(i, fds);
                }
            }
        }
    }
    return 1;
}

struct client* run_server(int socket_listener) {
    fd_set fds, read_fds;
    int largest_fd = socket_listener;
    int number_of_clients_added = 0;

    FD_ZERO(&fds);
    FD_SET(socket_listener, &fds);

    struct client *clients = malloc(sizeof(struct client) * MAX_NUM_CLIENTS);

    for (int i = 0; i < MAX_NUM_CLIENTS; ++i) {
        clients[i].fd = -1;
    }

    while (1) {
        printf("While");
        read_fds = fds;
        if (select(largest_fd+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            return NULL;
        }
        /* Go through all possible file descriptors */
        int ret = go_through_fds(socket_listener, &read_fds, &fds, &largest_fd, clients, &number_of_clients_added);
        if (ret == 0) {
            printf("All clients added!\n");

            char *send_thing = "All clients added, here is a message!\n";

            for (int j = 0; j < MAX_NUM_CLIENTS; j++) {
                struct client current = clients[j];
                ssize_t bytes = send(current.fd, send_thing, strlen(send_thing), 0);
            }
            return clients;
        }
    }
    return NULL;
}



int main(int argc, char *argv[]) {
    assert(argc == 3);

    BASEPORT = atoi(argv[1]);
    MAX_NUM_CLIENTS = atoi(argv[2]);

    int server_socket = create_server_socket();

    clients = run_server(server_socket);
    assert(clients != NULL);

    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        struct client current = clients[i];
        printf("Name: %s\nNumber of neighbors: %d\n\n", current.name, current.number_of_neighbors);
    }
    return 0;
}