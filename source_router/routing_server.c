
#include "../source_shared/protocol.h"

int BASEPORT;
int BACKLOG_SIZE = 10;
int MAX_NUM_CLIENTS;
int max_num_edges;

struct client *clients;

struct client {
    int client_socket;
    char *ip;
    unsigned short port;

    int own_address;
    int number_of_edges;
    struct edge *edges;
};

struct node *nodes;
struct edge *edges;

int number_of_nodes;
int number_of_edges;

void print_client(struct client client);
void print_edge(struct edge edge);

void remove_client(int client_socket) {
    for (int i = 0; i < MAX_NUM_CLIENTS; ++i) {
        if (clients[i].client_socket == client_socket) {
            clients[i].client_socket = -1;
            free(clients[i].ip);
            free(clients[i].edges);
            return;
        }
    }
}

int create_server_socket() {
    int ret = 0;
    int yes = 0;
    int server_socket = 1;
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
        if (clients[i].client_socket == -1) {
            clients[i].client_socket = socket;
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
void deconstruct_message(char *received_information, struct client *client) {

    struct edge *edges = calloc(client->number_of_edges, sizeof(struct edge));

    struct edge *edges_temp = (struct edge*) received_information;

    memcpy(edges, edges_temp, (client->number_of_edges) * sizeof(struct edge));

    client->edges = edges;
}

struct client* find_client_in_array(int client_socket) {
    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        struct client *current = &(clients[i]);
        if (current->client_socket == client_socket) {
            return current;
        }
    }
    return NULL;
}

size_t receive_information_fill_node(int client_socket) {
    size_t bytes = 0;

    int own_address = 0;
    bytes = receive_message(client_socket, &own_address, sizeof(int));
    
    int number_of_edges = 0;
    bytes = receive_message(client_socket, &number_of_edges, sizeof(int));

    size_t size_of_edges = number_of_edges * sizeof(struct edge);

    char information[size_of_edges];
    bytes = receive_message(client_socket, information, size_of_edges);

    if (bytes == -1) {
        printf("Something wrong with receive_information_fill_node\n");
        return -1;
    }
    if (bytes ==  0) {
        printf("Disconnection\n");
        return 0;
    }

    struct client *correct_client = find_client_in_array(client_socket);

    correct_client->own_address = own_address;
    correct_client->number_of_edges = number_of_edges;
    deconstruct_message(information, correct_client);

    return bytes;
}

int receive_from_client(int client_socket) {
    size_t bytes;

    bytes = receive_information_fill_node(client_socket);

    if (bytes == -1) {
        printf("Something wrong with receive_information_from_client\n");
        return -1;
    }
    if (bytes ==  0) {
        printf("Disconnection\n");
        return 0;
    }

    printf("Number of bytes: %zu\nFrom socket: %d\n", bytes, client_socket);

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
        bytes = send_message(current.client_socket, ALL_CLIENTS_CONNECTED, strlen(ALL_CLIENTS_CONNECTED));
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
        clients[i].client_socket = -1;
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
            print_edge(edges[i]);
            //delete edge all together
        }
    }
}

void add_all_edges_to_array() {
    int offset_in_array = 0;

    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        struct edge *edges_temp = clients[i].edges;
        for (int j = 0; j < clients[i].number_of_edges; j++) {
            edges[offset_in_array] = edges_temp[j];
            offset_in_array ++;
            number_of_edges++;
        }
    }
}

void print_client(struct client client) {
    printf("Own address: %d\t", client.own_address);
    printf("Number of edges: %d\n", client.number_of_edges);
    printf("Edges:\n");
    for (int i = 0; i < client.number_of_edges; i++) {
        printf("\tTo: %d\tFrom: %d\tWeight: %d\n", client.edges[i].to_address, client.edges[i].from_address, client.edges[i].weight);
    }
    printf("\n");
}

void print_edge(struct edge edge) {
    printf("To: %d\tFrom: %d\tWeight: %d\n", edge.to_address, edge.from_address, edge.weight);
}

int main(int argc, char *argv[]) {
    assert(argc == 3);

    BASEPORT = atoi(argv[1]);
    MAX_NUM_CLIENTS = atoi(argv[2]);

    int server_socket = create_server_socket();

    struct client clients_temp[MAX_NUM_CLIENTS];
    clients = clients_temp;

    number_of_edges = 0;

    max_num_edges = MAX_NUM_CLIENTS * MAX_NUM_CLIENTS;
    struct edge edges_temp[max_num_edges];
    edges = edges_temp;

    run_server(server_socket);
    send_all_clients_added();
    add_all_edges_to_array();

    check_two_way_edges();


    for (int i = 0; i < MAX_NUM_CLIENTS; i++) { //could be empty spots?
        struct client client = clients[i];
        print_client(client);
        
    }

    for (int j = 0; j < MAX_NUM_CLIENTS; j++) {
        remove_client(clients[j].client_socket);
    }
    return 0;
}