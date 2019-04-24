
#include "../source_shared/protocol.h"
#include "routing_server.h"
#include "../print_lib/print_lib.h"
#include <stdbool.h>
#include "dijkstra.h"
#include "client.h"

int BASEPORT;
int BACKLOG_SIZE = 10;
int max_num_edges;

int number_of_clients;

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

size_t deconstruct_header(int client_socket, int *own_address, int *number_of_edges) {
    size_t bytes = 0;
    bytes = receive_message(client_socket, own_address, sizeof(int));
    bytes = receive_message(client_socket, number_of_edges, sizeof(int));

    return bytes;
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
    int number_of_edges = 0;
    int own_address = 0;

    bytes = deconstruct_header(client_socket, &own_address, &number_of_edges);
    
    printf("NUMBER OF EDGES NODE %d: %d\n", own_address, number_of_edges);
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
    printf("Receiving from socket %d\n", client_socket);
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
                    (number_of_clients)++;
                    if (number_of_clients == MAX_NUM_CLIENTS) {
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

size_t send_all_clients_added() {
    printf("All clients added!\n");
    ssize_t bytes; 

    for (int j = 0; j < MAX_NUM_CLIENTS; j++) {
        struct client current = clients[j];
        bytes = send_message(current.client_socket, ALL_CLIENTS_CONNECTED, strlen(ALL_CLIENTS_CONNECTED));
        if (bytes == -1) {
            printf("Something wrong with send_message in send_all_clients_added\n");
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

int edges_are_two_ways(struct edge edge_a, struct edge edge_b) {
    int yes = 1;
    int no = 0;

    if (edge_a.to_address == edge_b.from_address) {
        if (edge_a.from_address == edge_b.to_address) {
            if (edge_a.weight == edge_b.weight) {
                return yes;
            }
        }
    }
    return no;
}

int edges_equal(struct edge edge_a, struct edge edge_b) {
    int yes = 1;
    int no = 0;

    if (edge_a.to_address == edge_b.to_address) {
        if (edge_a.from_address == edge_b.from_address) {
            if (edge_a.weight == edge_b.weight) {
                return yes;
            }
        }
    }
    return no;
}

int edge_goes_both_ways(struct edge edge) {
    int yes = 1;
    int no = 0;

    for (int i = 0; i < number_of_clients; i++) {
        struct client current_client = clients[i];

        for (int j = 0; j < current_client.number_of_edges; j++) {
            struct edge current_edge = current_client.edges[j];

            if (edges_are_two_ways(current_edge, edge) == 1) {
                return yes;
            }
        }
    }
    return no;
}

void delete_edge(struct edge edge, struct client *client) {

    for (int i = 0; i < client->number_of_edges; i++) {
        struct edge current_edge = client->edges[i];

        if (edges_equal(current_edge, edge) == 1) {
            client->edges[i] = client->edges[client->number_of_edges -1];
            client->number_of_edges --;
        }
    }
} 

void check_two_way_edges() {
    for (int i = 0; i < number_of_clients; i++) {
        struct client *current_client = &(clients[i]);

        for (int j = 0; j < current_client->number_of_edges; j++) {
            struct edge current_edge = current_client->edges[j];
            if (edge_goes_both_ways(current_edge) == 0) {
                delete_edge(current_edge, current_client);
            }
        }
    }
}

void print_client(struct client client) {
    fprintf(stdout, "Own address: %d\t", client.own_address);
    fprintf(stdout, "Number of edges: %d\n", client.number_of_edges);
    fprintf(stdout, "Edges:\n");
    for (int i = 0; i < client.number_of_edges; i++) {
        print_edge(client.edges[i]);
    }
    fprintf(stdout, "\n");
}

void print_edge(struct edge edge) {
    fprintf(stdout, "To: %d\tFrom: %d\tWeight: %d\n", edge.to_address, edge.from_address, edge.weight);
}

void free_all() {
    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        remove_client(clients[i].client_socket);
    }
    free(clients);
    free(array_of_dijkstra_nodes);
}

void print_all_combinations() {
    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        struct dijkstra_node current_from = array_of_dijkstra_nodes[i];
        const int address_from = current_from.client.own_address;
        const int from_weight = current_from.weight;

        for (int j = 0; j < MAX_NUM_CLIENTS; j++) {
            struct dijkstra_node current_to = array_of_dijkstra_nodes[j];
            int address_to = current_to.client.own_address;

            int *path = current_to.path;
            if (is_on_path(address_from, path, current_to.number_of_nodes_in_path) == false) {
                print_weighted_edge(address_from, address_to, -1);
            }
            else {
                print_weighted_edge(address_from, address_to, from_weight);
            }
        }
    }
}

void create_table_client(struct dijkstra_node client, struct table table[], int *number_of_entries) {
    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        struct dijkstra_node current = array_of_dijkstra_nodes[i];

        if (current.client.own_address == client.client.own_address) {
            continue;
        }

        int *path_of_current = current.path;
        int path_length = current.number_of_nodes_in_path;

        for (int j = 0; j < path_length-1; j++) {
            if (path_of_current[j] == client.client.own_address) {
                table[*number_of_entries].to_address = current.client.own_address;
                table[*number_of_entries].first_client_on_route = path_of_current[j+1];
                (*number_of_entries)++;
            }
        }
    }
}

void create_packet(char *buffer, struct table table[], int number_of_entries) {
    int offset_in_buffer = 0;

    memcpy(&(buffer[offset_in_buffer]), &number_of_entries, sizeof(int));

    for (int i = 0; i < number_of_entries; i++) {
        offset_in_buffer += sizeof(int);
        memcpy(&(buffer[offset_in_buffer]), &(table[i].to_address), sizeof(int));

        offset_in_buffer += sizeof(int);
        memcpy(&(buffer[offset_in_buffer]), &(table[i].first_client_on_route), sizeof(int));
    }
}

void send_table_client(struct dijkstra_node node, struct table table[], int number_of_entries) {
    int client_socket = node.client.client_socket;

    char *buffer = calloc(number_of_entries, sizeof(struct table));
    size_t size_of_message = sizeof(struct table) * number_of_entries + sizeof(int);

    create_packet(buffer, table, number_of_entries);

    send_message(client_socket, buffer, size_of_message);

    free(buffer);
}

void send_tables_all_clients() {
    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        struct dijkstra_node node_to_calculate_table_for = array_of_dijkstra_nodes[i];

        struct table table[MAX_NUM_CLIENTS];
        int number_of_entries = 0;

        create_table_client(node_to_calculate_table_for, table, &number_of_entries);

        send_table_client(node_to_calculate_table_for, table, number_of_entries);
    }
}

int main(int argc, char *argv[]) {
    assert(argc == 3);

    BASEPORT = atoi(argv[1]);
    MAX_NUM_CLIENTS = atoi(argv[2]);

    int server_socket = create_server_socket();

    clients = calloc(MAX_NUM_CLIENTS, sizeof(struct client));
    printf("Starting server\n");
    run_server(server_socket);
    printf("Finished server\n");

    printf("Starting to check two way edges\n");
    check_two_way_edges();
    printf("Finished\n");

    array_of_dijkstra_nodes = calloc(MAX_NUM_CLIENTS, sizeof(struct dijkstra_node));
    new_dijkstra();
    print_all_combinations();

    send_tables_all_clients();

    free_all();
    return 0;
}