
#include "../source_shared/protocol.h"
#include "routing_server.h"
#include "../print_lib/print_lib.h"

int BASEPORT;
int BACKLOG_SIZE = 10;
int MAX_NUM_CLIENTS;
int max_num_edges;

struct client *clients;

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

void print_all_edges_of_client(struct client client) {
    for (int i = 0; i < client.number_of_edges; i++) {
        struct edge current_edge = client.edges[i];
        print_edge(current_edge);
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

/*
Dijkstra:
*/

struct client *find_and_remove_client(int own_address, struct client unvisited[], int *number_of_clients_in_array) {
   struct client *client_to_remove = malloc(sizeof(struct client)); //TODO: is this scary

    for (int i = 0; i < *number_of_clients_in_array; i++) {
        struct client current = unvisited[i];
        if (current.own_address == own_address) {

            memcpy(client_to_remove, &current, sizeof(struct client));
            int last_index_in_array = (*number_of_clients_in_array) -1;

            unvisited[i] = unvisited[last_index_in_array];

            (*number_of_clients_in_array)--;
            return client_to_remove;
        }
    }
    free(client_to_remove);
    return NULL;
} 

void fill_table(struct table table[]) {
    for (int i = 0; i < number_of_clients; i++) {
        table[i].to_address = clients[i].own_address;
        table[i].weight = -1;
    }
}

void update_shortest_distance(struct table table[], int new_distance, int current_first_step) {
    if (table->weight > new_distance || table->weight == -1) {
        table->weight = new_distance;
        table->first_client_on_route = current_first_step;
    }
}

struct table *find_table_entry(int address, struct table calculated_table[], int number_of_entries) {
    for (int i = 0; i < number_of_entries; i++) {
        struct table *current = &(calculated_table[i]);
        if (current->to_address == address) {
            return current;
        }
    }
    return NULL;
}

int is_unvisited(int address, struct client unvisited[], int number_of_unvisited) {
    int yes = 1;
    int no = 0;
    for (int i = 0; i < number_of_unvisited; i++) {
        if (unvisited[i].own_address == address) {
            return yes;
        }
    }
    return no;
}

void update_shortest_distance_from(struct client client, struct client unvisited[], int number_of_unvisited, struct table calculated_table[], int current_first_step) {
    int current_distance = find_table_entry(client.own_address, calculated_table, number_of_clients)->weight;

    for (int i = 0; i < client.number_of_edges; i++) {
        struct edge current_edge = client.edges[i];

        int address = current_edge.to_address;

        if (is_unvisited(address, unvisited, number_of_unvisited) == 1) {
            struct table *table_entry = find_table_entry(address, calculated_table, number_of_clients);

            int new_distance = current_distance + current_edge.weight;
            update_shortest_distance(table_entry, new_distance, current_first_step);
        }
    }
}

void set_self_shortest_distance(int own_address, struct table calculated_table[]) {
    for (int i = 0; i < number_of_clients; i++) {
        if (calculated_table[i].to_address == own_address) {
            calculated_table[i].first_client_on_route = own_address;
            calculated_table[i].weight = 0;
            return;
        }
    }
}

void print_table(struct table table[]) {
    for (int i = 0; i < number_of_clients; i++) {
        struct table current = table[i];
        fprintf(stdout, "To: %d\tFirst step: %d\tWeight: %d\n", current.to_address, current.first_client_on_route, current.weight);
    }
}

void set_table(struct table calculated_table[], int own_address) {
    fill_table(calculated_table);
    set_self_shortest_distance(own_address, calculated_table); //these two could be merged
}

int find_smallest_weight_edge(struct client client, struct client unvisited[], int number_of_unvisited, int *address_of_client_to_visit) {
    struct edge *edges = client.edges;
    if (client.number_of_edges == 0) {
        return -1;
    }

    int smallest = 1000;

    for (int i = 0; i < client.number_of_edges; i++) {
        if (edges[i].weight < smallest && is_unvisited(edges[i].to_address, unvisited, number_of_unvisited)) {
            smallest = edges[i].weight;
            *address_of_client_to_visit = edges[i].to_address;

        }
    }
    return smallest;
}

int address_of_client_to_visit(struct client visited[], int number_of_visited, struct client unvisited[], int number_of_unvisited, int *current_first_step, int starting_address) {
    int address_of_client_to_visit = -1;
    int temp_address_of_client_to_visit = -1;

    int smallest_weight_edge = 1000;

    int keep_current_first_step = 1;
    int could_be_new_first_step = *current_first_step;

    for (int i = 0; i < number_of_visited; i++) {
        struct client temp = visited[i];
        int smallest = find_smallest_weight_edge(temp, unvisited, number_of_unvisited, &temp_address_of_client_to_visit);
        if (smallest != -1 && smallest < smallest_weight_edge) {
            smallest_weight_edge = smallest;
            address_of_client_to_visit = temp_address_of_client_to_visit;

            if (temp.own_address == starting_address) {
                could_be_new_first_step = address_of_client_to_visit;
                keep_current_first_step = 0;
            }
            else {
                keep_current_first_step = 1;
            }
        }
    }

    if (keep_current_first_step == 0) {
        *current_first_step = could_be_new_first_step;
    }

    return address_of_client_to_visit;
}

struct table *dijkstra_from_client(int own_address) { //is going to return struct table *
    struct client *unvisited;
    unvisited = calloc(number_of_clients, sizeof(struct client));
    memcpy(unvisited, clients, number_of_clients * sizeof(struct client));

    struct client visited[number_of_clients];

    struct table *calculated_table;
    calculated_table = calloc(number_of_clients, sizeof(struct table));

    set_table(calculated_table, own_address);

    int current_first_step = own_address;

    int number_of_unvisited_clients = number_of_clients;
    int number_of_visited_clients = 0;

    struct client *client_to_calculate = find_and_remove_client(own_address, unvisited, &number_of_unvisited_clients); //this one is malloced

    visited[number_of_visited_clients] = *client_to_calculate; 
    number_of_visited_clients++;

    update_shortest_distance_from(*client_to_calculate, unvisited, number_of_unvisited_clients, calculated_table, current_first_step);
    free(client_to_calculate);

    int next_address = 0;
    struct client *next_client;
    while (number_of_unvisited_clients > 0) {
        next_address = address_of_client_to_visit(visited, number_of_visited_clients, unvisited, number_of_unvisited_clients, &current_first_step, own_address);

        next_client = find_and_remove_client(next_address, unvisited, &number_of_unvisited_clients); 
        visited[number_of_visited_clients] = *next_client;
        number_of_visited_clients++;

        update_shortest_distance_from(*next_client, unvisited, number_of_unvisited_clients, calculated_table, current_first_step);

        free(next_client); //scary?
    }

    free(unvisited);
    return calculated_table;
}

struct table *find_table(int address, struct table *all_tables[]) {
    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        struct table *table = all_tables[i];
        struct table *entry = find_table_entry(address, table, MAX_NUM_CLIENTS);
        if (entry->weight == 0 && entry->first_client_on_route == address) {
            return table;
        }
    }
    return NULL;
}

void all_clients_on_path_from_start_to_end(struct table *all_tables[], int address_end, int *length_of_path, int addresses_on_path[]){
    int offset_in_addresses = 0;
    int current_address = 1;

    struct table *current_table = find_table(current_address, all_tables);

    struct table *entry; 
    while (1) {
        addresses_on_path[offset_in_addresses] = current_address;
        offset_in_addresses++;

        entry = find_table_entry(address_end, current_table, MAX_NUM_CLIENTS);
        if (entry->first_client_on_route == current_address) {
            addresses_on_path[offset_in_addresses] = address_end;
            break;
        }
        current_address = entry->first_client_on_route;
        current_table = find_table(current_address, all_tables);
    }

    *length_of_path = offset_in_addresses + 1;
}
/*
End of Dijkstra
*/

int check_if_address_is_on_route(int address, int *path, int path_length) {
    int yes = 1;
    int no = 0;

    for (int i = 0; i < path_length; i++) {
        if (address == path[i]) {
            return yes;
        }
    }
    return no;
}

void update_table(int address_table, int address_end, int *path, int path_length, struct table *table) {
    if (check_if_address_is_on_route(address_table, path, path_length) == 0) {
        struct table *entry = find_table_entry(address_end, table, MAX_NUM_CLIENTS);
        entry->weight = -1;
    }
}

void calculate_tables(struct table *all_tables[]) {
    int offset_in_all_tables = 0;

    struct client temp;
    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        temp = clients[i];
        all_tables[offset_in_all_tables] = dijkstra_from_client(temp.own_address);
        offset_in_all_tables++;
    
    }
    
    int *path;
    int path_length;
    for (int j = 0; j < MAX_NUM_CLIENTS; j++) {
        all_clients_on_path_from_start_to_end(all_tables, clients[j].own_address, &path_length, path);

        for (int k = 0; k < path_length; k++) {
            fprintf(stdout, "%d -> ", path[k]);
        }
        fprintf(stdout, "\n");
    }

    for (int l = 0; l < MAX_NUM_CLIENTS; l++) {
        struct table *table_to_update = find_table(clients[l].own_address, all_tables);

        for (int m = 0; m < MAX_NUM_CLIENTS; m++) {
            all_clients_on_path_from_start_to_end(all_tables, clients[m].own_address, &path_length, path);
            update_table(clients[l].own_address, clients[m].own_address, path, path_length, table_to_update);
            print_weighted_edge((short) clients[l].own_address, clients[m].own_address, find_table_entry(clients[m].own_address, table_to_update, MAX_NUM_CLIENTS)->weight);
        }
    }
}

int count_valid_table_entries(struct table table[]) {
    int counter = 0;

    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        struct table current = table[i];
        if (current.weight != -1) {
            counter++;
        }
    }
    return counter;
}

void create_table_packet(char *packet, struct table table[], int *number_of_entries) {
    int offset_in_packet;
    memcpy(&(packet[0]), number_of_entries, sizeof(size_t));

    offset_in_packet = sizeof(int);
    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        struct table entry = table[i];
        if (entry.weight != -1) {
            memcpy(&(packet[offset_in_packet]), &(entry.to_address), sizeof(int));
            offset_in_packet += sizeof(int);

            memcpy(&(packet[offset_in_packet]), &(entry.first_client_on_route), sizeof(int));
            offset_in_packet += sizeof(int);            
        }
    }
}

void send_table_to_client(int client_socket, struct table table[]) {
    int number_of_valid_entries = count_valid_table_entries(table);
    size_t total_size_of_packet = number_of_valid_entries * (sizeof(int) * 2) + sizeof(int);
    char packet[total_size_of_packet];
    create_table_packet(packet, table, &number_of_valid_entries);

    send_message(client_socket, packet, total_size_of_packet);
}

void send_table_all_clients(struct table *all_tables[]) {
    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        struct client current = clients[i];
        if (current.client_socket != -1) {
            struct table *table = find_table(current.own_address, all_tables);

            send_table_to_client(current.client_socket, table);

        }
    }
}

void print_all_clients_and_tables(struct table *all_tables[]) {
    fprintf(stdout, "Printing all clients.\n");
    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        print_client(clients[i]);
        fprintf(stdout, "\n\n");
    }

    fprintf(stdout, "Printing all calculated tables.\n");
    for (int j = 0; j < MAX_NUM_CLIENTS; j++) {
        print_table(all_tables[j]);
        fprintf(stdout, "\n\n");
    }

}

void free_all(struct table *all_tables[]) {
    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        remove_client(clients[i].client_socket);
        free(all_tables[i]);
    }
    free(clients);
}

int main(int argc, char *argv[]) {
    assert(argc == 3);

    BASEPORT = atoi(argv[1]);
    MAX_NUM_CLIENTS = atoi(argv[2]);

    int server_socket = create_server_socket();

    clients = calloc(MAX_NUM_CLIENTS, sizeof(struct client));

    run_server(server_socket);
    
    check_two_way_edges();

    struct table *all_tables[MAX_NUM_CLIENTS];

    calculate_tables(all_tables);

    send_table_all_clients(all_tables);
    
    print_all_clients_and_tables(all_tables);

    free_all(all_tables);
    return 0;
}