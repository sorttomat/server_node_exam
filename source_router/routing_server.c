
#include "../source_shared/protocol.h"
#include "../print_lib/print_lib.h"
#include <stdbool.h>
#include "dijkstra.h"
#include "client.h"

int baseport;
int backlog_size = 10;
int max_num_edges;

int number_of_clients;

void print_client(struct client client);
void print_edge(struct edge edge);


/*
Removing client by closing socket, freeing all memory and resetting client_socket.
*/
void remove_client(int client_socket) {
    for (int i = 0; i < max_num_clients; ++i) {
        if (clients[i].client_socket == client_socket) {
            close(client_socket);
            clients[i].client_socket = -1;
            free(clients[i].ip);
            free(clients[i].edges);
            return;
        }
    }
}

/*
Creating server socket, binding it to the port with value of baseport.
*/
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
    server_addr.sin_port = htons(baseport);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    ret = bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret == 1) {
        perror("Could not bind to address");
        exit(EXIT_FAILURE);
    }

    ret = listen(server_socket, backlog_size);
    if (ret == -1) {
        perror("Could not listen");
        exit(EXIT_FAILURE);
    }
    return server_socket;
}

/*
Adding new client to client array (clients), sending message (corresponds to "hand_shake" in create_sockets.c)
*/
int add_client(int socket, struct sockaddr_in *client_addr, socklen_t addrlen) {
    char *ip_buffer = malloc(16);
    int i;

    if (!inet_ntop(client_addr->sin_family, &(client_addr->sin_addr), ip_buffer, addrlen)) {
        perror("inet_ntop");
        strcpy(ip_buffer, "N/A");
    }

    for (i = 0; i < max_num_clients; ++i) {
        if (clients[i].client_socket == -1) {
            clients[i].client_socket = socket;
            clients[i].ip = ip_buffer;
            clients[i].port = ntohs(client_addr->sin_port);

            send_message(socket, RESPONSE_SUCCESS, strlen(RESPONSE_SUCCESS));
            return 0;
        }
    }

    send_message(socket, RESPONSE_FULL, strlen(RESPONSE_FULL));
    free(ip_buffer);
    close(socket);
    return 1;
}

/*
Accepting new client 
*/
int accept_new_client(int socket_listener) {
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    int client_socket = accept(socket_listener, (struct sockaddr*) &client_addr, &addrlen);
    if (client_socket == -1) {
        perror("accept");
        return -1;
    }

    if (add_client(client_socket, &client_addr, addrlen)) {
        return -1;
    }
    return client_socket;
}

/*
Deconstructing message received from node, minus header (header is deconstructed in deconstruct_header).
I know one should probably not assume that information_edge_structs can just be casted to (struct edge *) (because of possible padding in structs), but I think it's SO COOL.
*/
void deconstruct_message(char *information_edge_structs, struct client *client) {

    struct edge *edges = calloc(client->number_of_edges, sizeof(struct edge));

    struct edge *edges_temp = (struct edge*) information_edge_structs;

    memcpy(edges, edges_temp, (client->number_of_edges) * sizeof(struct edge));

    client->edges = edges;
}

/*
Deconstructing header.
Receiving from client_socket, putting the received information directly into own_address and number_of_edges (these are pointers, so can be modified)
*/
ssize_t deconstruct_header(int client_socket, int *own_address, int *number_of_edges) {
    ssize_t bytes = 0;
    bytes = receive_message(client_socket, own_address, sizeof(int));
    if (bytes == -1) {
        printf("Something wrong with deconstruct_header\n");
        return -1;
    }
    if (bytes ==  0) {
        printf("Disconnection\n");
        return 0;
    }

    bytes = receive_message(client_socket, number_of_edges, sizeof(int));
    if (bytes == -1) {
        printf("Something wrong with deconstruct_header\n");
        return -1;
    }
    if (bytes ==  0) {
        printf("Disconnection\n");
        return 0;
    }
    return bytes;
}

/*
Finding client in array. Clients are distinguished by client_socket.
*/
struct client* find_client_in_array(int client_socket) {
    for (int i = 0; i < max_num_clients; i++) {
        struct client *current = &(clients[i]);
        if (current->client_socket == client_socket) {
            return current;
        }
    }
    return NULL;
}

/*
Receive all information from node with client socket = client_socket.
Using client_socket to locate tha correct client from array og clients (clients).
*/
ssize_t receive_information_fill_node(int client_socket) {
    ssize_t bytes = 0;
    int number_of_edges = 0;
    int own_address = 0;

    bytes = deconstruct_header(client_socket, &own_address, &number_of_edges);
    if (bytes == -1) {
        printf("Something wrong with deconstruct_header\n");
        return -1;
    }
    if (bytes ==  0) {
        printf("Disconnection\n");
        return 0;
    }
    
    size_t size_of_edges = number_of_edges * sizeof(struct edge);

    char information_edges[size_of_edges];
    bytes = receive_message(client_socket, information_edges, size_of_edges);

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
    deconstruct_message(information_edges, correct_client);

    return bytes;
}

/*
Receiving from client that already is connected.
If 1 is returned: router has successfully received information.
*/
int receive_from_client(int client_socket) {
    ssize_t bytes;

    bytes = receive_information_fill_node(client_socket);

    if (bytes == -1) {
        printf("Something wrong with receive_information_fill_node\n");
        return -1;
    }
    if (bytes ==  0) {
        printf("Disconnection\n");
        return 0;
    }
    return 1;
}

/*
Going through all file descriptors, checking if there is action (if the fd is set).
Either accepting new client, or receiving from existing client.
If receiving successfully from existing client: number_of_clients is one more.
If all clients has connected and sent information, 1 is returned.
*/
int go_through_fds(int socket_listener, fd_set *read_fds, fd_set *fds, int *largest_fd) {
    for (int i = 0; i <= *largest_fd; ++i) {
        if (FD_ISSET(i, read_fds)) {
            if (i == socket_listener) {
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
                int ret = receive_from_client(i);
                if (ret == -1) {
                    printf("Error in receive_from_client\n");
                    return -1;
                }
                
                if (ret == 1) {
                    (number_of_clients)++;
                    if (number_of_clients == max_num_clients) {
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

/*
Setting client socket of all clients to -1.
Running server while ret != 1.
ret is 1 when all clients has connected and sent information.
*/
void run_server(int socket_listener) {
    fd_set fds, read_fds;
    int largest_fd = socket_listener;

    FD_ZERO(&fds);
    FD_SET(socket_listener, &fds);

    for (int i = 0; i < max_num_clients; ++i) {
        clients[i].client_socket = -1;
    }

    int ret = 0;
    while (ret != 1) {
        read_fds = fds;
        if (select(largest_fd+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            return;
        }

        ret = go_through_fds(socket_listener, &read_fds, &fds, &largest_fd);
        if (ret == -1) {
            printf("Error in go_through_fds\n");
        }
    }
}


/*
Checking if two edge structs contain the "same" edge (same but opposite).
*/
bool edges_are_two_ways(struct edge edge_a, struct edge edge_b) {
    if (edge_a.to_address == edge_b.from_address) {
        if (edge_a.from_address == edge_b.to_address) {
            if (edge_a.weight == edge_b.weight) {
                return true;
            }
        }
    }
    return false;
}

/*
Checking if two edge structs are identical.
*/
bool edges_equal(struct edge edge_a, struct edge edge_b) {
    if (edge_a.to_address == edge_b.to_address) {
        if (edge_a.from_address == edge_b.from_address) {
            if (edge_a.weight == edge_b.weight) {
                return true;
            }
        }
    }
    return false;
}

/*
For every edge struct (in every client struct), check if an opposite edge struct exists.
*/
bool edge_goes_both_ways(struct edge edge) {
    for (int i = 0; i < number_of_clients; i++) {
        struct client current_client = clients[i];

        for (int j = 0; j < current_client.number_of_edges; j++) {
            struct edge current_edge = current_client.edges[j];

            if (edges_are_two_ways(current_edge, edge) == true) {
                return true;
            }
        }
    }
    return false;
}

/*
Deleting edge struct.
Edge structs are in array.
I find the correct edge struct, overwrite it by putting another edge struct in that spot in the array (the last edge struct), and 
decrement number_of_edges. 
*/
void delete_edge(struct edge edge, struct client *client) {
    for (int i = 0; i < client->number_of_edges; i++) {
        struct edge current_edge = client->edges[i];

        if (edges_equal(current_edge, edge) == true) {
            client->edges[i] = client->edges[client->number_of_edges -1];
            client->number_of_edges --;
        }
    }
} 

/*
Going through every edge struct, checking if an opposite edge struct exists, deleting it if not.
*/
void check_two_way_edges() {
    for (int i = 0; i < number_of_clients; i++) {
        struct client *current_client = &(clients[i]);

        for (int j = 0; j < current_client->number_of_edges; j++) {
            struct edge current_edge = current_client->edges[j];
            if (edge_goes_both_ways(current_edge) == false) {
                printf("Deleting edge: \n");
                print_edge(current_edge);
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
    fprintf(stdout, "\n\n");
}

void print_edge(struct edge edge) {
    fprintf(stdout, "To: %d\tFrom: %d\tWeight: %d\n", edge.to_address, edge.from_address, edge.weight);
}

void free_all() {
    for (int i = 0; i < max_num_clients; i++) {
        remove_client(clients[i].client_socket);
        free(array_of_dijkstra_nodes[i].path);
    }
    free(clients);
    free(array_of_dijkstra_nodes);
}

/*
Printing out every combination of addresses.
*/
void print_all_combinations() {
    for (int i = 0; i < max_num_clients; i++) {
        struct dijkstra_node current_from = array_of_dijkstra_nodes[i];
        const int address_from = current_from.client.own_address;
        const int from_weight = current_from.weight;

        for (int j = 0; j < max_num_clients; j++) {
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

/*
Creating correct table for client.
Will consist of pairs of (destination address:first client on route).

Each dijkstra_node struct contains a client, and also that clients' path from node 1 to itself.
I use these paths to determine the table for client.
*/
void create_table_client(struct dijkstra_node client, struct table table[], int *number_of_entries) {
    for (int i = 0; i < max_num_clients; i++) {
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


/*
Creating packet.
Packet will consist of a header with number of table entries, and then all of the table entries.
*/
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

/*
Send table to client.
dijkstra_node struct contains client, from which client_socket is retrieved.
*/
void send_table_client(struct dijkstra_node node, struct table table[], int number_of_entries) {
    int client_socket = node.client.client_socket;

    char buffer[number_of_entries * sizeof(struct table)];

    size_t size_of_message = sizeof(struct table) * number_of_entries + sizeof(int);

    create_packet(buffer, table, number_of_entries);

    send_message(client_socket, buffer, size_of_message);
}

/*
For every node, send table.
*/
void send_tables_all_clients() {
    for (int i = 0; i < max_num_clients; i++) {
        struct dijkstra_node node_to_calculate_table_for = array_of_dijkstra_nodes[i];

        struct table table[max_num_clients];
        int number_of_entries = 0;

        create_table_client(node_to_calculate_table_for, table, &number_of_entries);

        send_table_client(node_to_calculate_table_for, table, number_of_entries);
    }
}

int main(int argc, char *argv[]) {
    assert(argc == 3);

    baseport = atoi(argv[1]);
    max_num_clients = atoi(argv[2]);

    int server_socket = create_server_socket();

    clients = calloc(max_num_clients, sizeof(struct client));
    run_server(server_socket);

    check_two_way_edges();

    array_of_dijkstra_nodes = calloc(max_num_clients, sizeof(struct dijkstra_node));
    dijkstra();
    print_all_combinations();

    send_tables_all_clients();

    close(server_socket);
    free_all();
    return 0;
}
