
#include "../source_shared/protocol.h"

int BASEPORT;
int BACKLOG_SIZE = 10;
int MAX_NUM_CLIENTS;

struct client *clients;

struct client {
    int fd;
    char *ip;
    unsigned short port;
    char *name;
    int number_of_neighbors;
};

void remove_client(int fd) {
    int i;

    for (i = 0; i < MAX_NUM_CLIENTS; ++i) {
        if (clients[i].fd == fd) {
            clients[i].fd = -1;
            free(clients[i].ip);
            free(clients[i].name);
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

    server_addr.sin_family = AF_INET; 
    server_addr.sin_port = htons(BASEPORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
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

int add_client(int socket, struct sockaddr_in *client_addr, socklen_t addrlen) {
    printf("Adding client\n");
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
            clients[i].name = NULL;

            printf("Sending message_success\n");
            send_message(socket, RESPONSE_SUCCESS, strlen(RESPONSE_SUCCESS));
            return 0;
        }
    }

    /* We don't have capacity to handle this client*/
    send_message(socket, RESPONSE_FULL, strlen(RESPONSE_FULL));
    free(ip_buffer);
    close(socket);
    return 1;
}

int accept_new_client(int socket_listener) {
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    int client_socket = accept(socket_listener, (struct sockaddr*) &client_addr, &addrlen);
    printf("client accepted");
    if (client_socket == -1) {
        perror("accept");
        return -1;
    }

    if (add_client(client_socket, &client_addr, addrlen)) {
        return -1;
    }
    return client_socket;
}

int receive_size(int client_socket, ssize_t *total_size) {
    ssize_t recv_bytes = 0;
    ssize_t remaining_recv = 0;

    recv_bytes = receive_message(client_socket, total_size, sizeof(int));
    if (recv_bytes == -1) {
        printf("Something wrong with recv\n");
        return -1;
    }
    if (recv_bytes ==  0) {
        printf("Disconnection\n");
        return 0;
    }
    
    return recv_bytes;
}

ssize_t receive_information_from_client(int client_socket, char buffer[]) {
    ssize_t size_of_message;
    ssize_t bytes;

    bytes = receive_size(client_socket, &size_of_message);
    if (bytes == -1) {
        printf("Something wrong with receive_size\n");
        return -1;
    }
    if (bytes ==  0) {
        printf("Disconnection\n");
        return 0;
    }

    bytes = receive_message(client_socket, buffer, size_of_message);
    if (bytes == -1) {
        printf("Something wrong with receive_size\n");
        return -1;
    }
    if (bytes ==  0) {
        printf("Disconnection\n");
        return 0;
    }
    return bytes;
}

int receive_from_client(int client_socket) {
    int max_size_recv = 1000;
    int recv_bytes = 0;
    ssize_t bytes;
    char *message;

    bytes = receive_information_from_client(client_socket, message);

    if (bytes == -1) {
        printf("Something wrong with receive_information_from_client\n");
        return -1;
    }
    if (bytes ==  0) {
        printf("Disconnection\n");
        return 0;
    }

    printf("Number of bytes: %zu\nFrom socket: %d\n", bytes, client_socket);
    message[bytes] = '\0';
    printf("Message: %s\n", message);

    for (int j = 0; j < MAX_NUM_CLIENTS; j++) {
        if (clients[j].fd == client_socket) {
            clients[j].name = message;
        }
    }
    return 1;
}

int go_through_fds(int socket_listener, fd_set *read_fds, fd_set *fds, int *largest_fd, int *number_of_clients_added) {
        /* Go through all possible file descriptors */
    for (int i = 0; i <= *largest_fd; ++i) {
        if (FD_ISSET(i, read_fds)) {
            printf("Looking at socket %d\n", i);
            if (i == socket_listener) {
                /* Accept new client */
                printf("Accepting new client\n");
                int client_socket = accept_new_client(socket_listener);

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
                int ret = receive_from_client(i);

                if (ret == -1) {
                    printf("Error in receive_from_client\n");
                    //clean up?
                    return -1;
                }
                
                if (ret == 1) {
                    (*number_of_clients_added)++;
                    printf("%d\n", *number_of_clients_added);
                    if (*number_of_clients_added == MAX_NUM_CLIENTS) {
                        return 1;
                    }

                } else if (ret == 0) {
                    printf("Client disconnected :(\n");
                    FD_CLR(i, fds);

                    remove_client(i);
                }
            }
        }
    }
    return 0;
}

void run_server(int socket_listener) {
    fd_set fds, read_fds;
    int largest_fd = socket_listener;
    int number_of_clients_added = 0;

    FD_ZERO(&fds);
    FD_SET(socket_listener, &fds);

    for (int i = 0; i < MAX_NUM_CLIENTS; ++i) {
        clients[i].fd = -1;
    }

    int ret = 0;
    while (ret != 1) {
        read_fds = fds;
        if (select(largest_fd+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            return;
        }

        /* Go through all possible file descriptors */
        ret = go_through_fds(socket_listener, &read_fds, &fds, &largest_fd, &number_of_clients_added);
        if (ret == -1) {
            printf("Error in go_through_fds\n");
            //clean up?
            //exit?
        }
    }
    
    printf("All clients added!\n");

    for (int j = 0; j < MAX_NUM_CLIENTS; j++) {
        struct client current = clients[j];
        ssize_t bytes = send(current.fd, ALL_CLIENTS_CONNECTED, strlen(ALL_CLIENTS_CONNECTED), 0);
    }
}

int main(int argc, char *argv[]) {
    assert(argc == 3);

    BASEPORT = atoi(argv[1]);
    MAX_NUM_CLIENTS = atoi(argv[2]);

    int server_socket = create_server_socket();

    struct client clients_temp[MAX_NUM_CLIENTS];
    clients = clients_temp;

    run_server(server_socket);

    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        struct client current = clients[i];
        if (current.fd != -1) {
            printf("Name: %s\n\n", current.name);
        }
    }
    return 0;
}