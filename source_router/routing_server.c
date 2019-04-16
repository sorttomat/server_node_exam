
#include "../source_shared/protocol.h"

int BASEPORT;
int BACKLOG_SIZE = 10;
int MAX_NUM_CLIENTS;
int max_num_edges;

struct client *clients;

struct client {
    int fd;
    char *ip;
    unsigned short port;
};

struct node *nodes;
struct edge *edges;

int number_of_nodes;
int number_of_edges;

void remove_client(int fd) {
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
void deconstruct_message(char *received_information, struct node *node) {
    deconstruct_header(received_information, node);
    int start_of_edges_in_buffer = sizeof(int) * 2;

    int edge_counter = 0;
    int place_edge_in_buffer = start_of_edges_in_buffer;

    struct edge *edges = calloc(node->number_of_edges, sizeof(struct edge));

    while (edge_counter < node->number_of_edges) {
        memcpy(&(edges[edge_counter].from_address), &(received_information[place_edge_in_buffer]), sizeof(int));
        place_edge_in_buffer += sizeof(int);
        memcpy(&(edges[edge_counter].to_address), &(received_information[place_edge_in_buffer]), sizeof(int));
        place_edge_in_buffer += sizeof(int);
        memcpy(&(edges[edge_counter].weight), &(received_information[place_edge_in_buffer]), sizeof(int));
        place_edge_in_buffer += sizeof(int);
        edge_counter++;
    }

    node->edges = edges;
}

size_t receive_information_fill_node(int client_socket, struct node *node) {
    size_t bytes = 0;

    size_t size_of_message = 0;
    bytes = receive_message(client_socket, &size_of_message, sizeof(int));

    if (bytes == -1) {
        printf("Something wrong with receive_information_fill_node\n");
        return -1;
    }
    if (bytes ==  0) {
        printf("Disconnection\n");
        return 0;
    }

    char information[size_of_message];
    bytes = receive_message(client_socket, information, size_of_message);

    if (bytes == -1) {
        printf("Something wrong with receive_information_fill_node\n");
        return -1;
    }
    if (bytes ==  0) {
        printf("Disconnection\n");
        return 0;
    }

    deconstruct_message(information, node);
    node->client_socket = client_socket;

    return bytes;
}

int receive_from_client(int client_socket) {
    size_t bytes;
    struct node node;

    bytes = receive_information_fill_node(client_socket, &node);

    if (bytes == -1) {
        printf("Something wrong with receive_information_from_client\n");
        return -1;
    }
    if (bytes ==  0) {
        printf("Disconnection\n");
        return 0;
    }

    printf("Number of bytes: %zu\nFrom socket: %d\n", bytes, client_socket);

    nodes[number_of_nodes] = node;

    return 1;
}

int go_through_fds(int socket_listener, fd_set *read_fds, fd_set *fds, int *largest_fd) {
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
                    (number_of_nodes)++;
                    printf("%d\n", number_of_nodes);
                    if (number_of_nodes == MAX_NUM_CLIENTS) {
                        return 1;
                    }

                } else if (ret == 0) {
                    printf("Client disconnected :(\n");
                    FD_CLR(i, fds);

                    remove_client(i);
                    //remove node from nodes
                }
            }
        }
    }
    return 0;
}

size_t send_all_clients_added() {
    printf("All clients added!\n");
    ssize_t bytes; 

    for (int j = 0; j < MAX_NUM_CLIENTS; j++) {
        struct client current = clients[j];
        bytes = send_message(current.fd, ALL_CLIENTS_CONNECTED, strlen(ALL_CLIENTS_CONNECTED));
        if (bytes == -1) {
            printf("Something wrong with receive_information_from_client\n");
            return -1;
        }
        if (bytes ==  0) {
            printf("Disconnection\n");
            return 0;
        }
    }
    return bytes;
}

void run_server(int socket_listener) {
    fd_set fds, read_fds;
    int largest_fd = socket_listener;


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
        ret = go_through_fds(socket_listener, &read_fds, &fds, &largest_fd);
        if (ret == -1) {
            printf("Error in go_through_fds\n");
            //clean up?
            //exit?
        }
    }
}

void print_node(struct node node) {
    printf("Own address: %d\n", node.own_address);
    printf("Number of edges: %d\n", node.number_of_edges);
    printf("Edges:\n");
    for (int i = 0; i < node.number_of_edges; i++) {
        printf("To: %d\nFrom: %d\nWeight: %d\n", node.edges[i].to_address, node.edges[i].from_address, node.edges[i].weight);
    }
}

void print_edge(struct edge edge) {
    printf("To: %d\nFrom: %d\nWeight: %d\n", edge.to_address, edge.from_address, edge.weight);
}

int count_number_same_edge(struct edge edge) {
    int counter = 0;

    for (int i = 0; i < number_of_edges; i++) {
        struct edge current = edges[i];
        if (current.to_address == edge.from_address) {
            if (current.from_address == edge.to_address) {
                if (current.weight == edge.weight) {
                    counter += 1;
                }
            }
        }
    }
    return counter;
}

void check_two_way_edges() {
    printf("Checking two way edges\n");
    int count = 0;

    for (int i = 0; i < number_of_edges; i++) {
        count = count_number_same_edge(edges[i]);
        if (count < 1) {
            printf("Too few of this edge!\n");
            printf("To: %d\nFrom: %d\nWeight: %d\n", edges[i].to_address, edges[i].from_address, edges[i].weight);
            //delete edge all together
        }
    }
}

void add_all_edges_to_array() {
    int offset_in_array = 0;

    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        struct edge *edges_temp = nodes[i].edges;
        for (int j = 0; j < nodes[i].number_of_edges; j++) {
            edges[offset_in_array] = edges_temp[j];
            offset_in_array ++;
            number_of_edges++;
        }
    }
}

int main(int argc, char *argv[]) {
    assert(argc == 3);

    BASEPORT = atoi(argv[1]);
    MAX_NUM_CLIENTS = atoi(argv[2]);

    int server_socket = create_server_socket();

    struct client clients_temp[MAX_NUM_CLIENTS];
    clients = clients_temp;

    struct node nodes_temp[MAX_NUM_CLIENTS];
    nodes = nodes_temp;
    number_of_nodes = 0;
    number_of_edges = 0;

    max_num_edges = MAX_NUM_CLIENTS * MAX_NUM_CLIENTS;
    struct edge edges_temp[max_num_edges];
    edges = edges_temp;
    run_server(server_socket);
    send_all_clients_added();
    add_all_edges_to_array();

    check_two_way_edges();


    // for (int i = 0; i < MAX_NUM_CLIENTS; i++) { //could be empty spots?
    //     struct node node = nodes[i];
    //     print_node(node);
    // }
    return 0;
}