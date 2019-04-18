// #include "routing_server.h"
// #include "../source_shared/protocol.h"
#include <stddef.h>

struct client *find_and_remove_client(int own_address, struct client *clients, int *number_of_clients_in_array) {
    for (int i = 0; i < *number_of_clients_in_array; i++) {
        struct client *current = &(clients[i]);
        if (current->own_address == own_address) {
            clients[i] = clients[*(number_of_clients_in_array-1)];
            (*number_of_clients_in_array)--;
            return current;
        }
    }
    return NULL;
} 

void fill_table(struct table table[], struct client clients[], int number_of_clients) {
    for (int i = 0; i < number_of_clients; i++) {
        table[i].to_address = clients[i].own_address;
        table[i].weight = -1;
    }
}

void update_shortest_distance(struct table table, int new_distance, int current_first_step) {
    if (table.weight > new_distance || table.weight == -1) {
        table.weight = new_distance;
        table.first_client_on_route = current_first_step;
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

void update_shortest_distance_from(struct client client, struct client clients[], int number_of_clients, struct table calculated_table[], int current_first_step) {
    int current_distance = find_table_entry(client.own_address, calculated_table, number_of_clients)->weight;
    for (int i = 0; i < client.number_of_edges; i++) {
        struct edge current_edge = client.edges[i];

        int address = current_edge.to_address;

        struct table *table_entry = find_table_entry(address, calculated_table, number_of_clients);

        int new_distance = current_distance + current_edge.weight;
        update_shortest_distance(*table_entry, new_distance, current_first_step);
    }
}

void set_self_shortest_distance(int own_address, struct table calculated_table[], int number_of_entries) {
    for (int i = 0; i < number_of_entries; i++) {
        if (calculated_table[i].to_address == own_address) {
            calculated_table[i].weight = 0;
            return;
        }
    }
}


void dijkstra_from_client(int own_address, struct client clients[], int number_of_clients) { //is going to return struct table *
    struct client *unvisited;
    unvisited = calloc(number_of_clients, sizeof(struct client));
    memcpy(unvisited, clients, number_of_clients * sizeof(struct client));

    struct client visited[number_of_clients];

    int current_first_step = own_address;

    struct table *calculated_table;
    calculated_table = calloc(number_of_clients, sizeof(struct table));

    fill_table(calculated_table, unvisited, number_of_clients);
    set_self_shortest_distance(own_address, calculated_table, number_of_clients); //these two could be merged

    int number_of_unvisited_clients = number_of_clients;
    int number_of_visited_clients = 0;

    struct client *client_to_calculate = find_and_remove_client(own_address, unvisited, &number_of_unvisited_clients);

    visited[number_of_visited_clients] = *client_to_calculate; 
    number_of_visited_clients++;

    update_shortest_distance_from(*client_to_calculate, clients, number_of_clients, calculated_table, current_first_step);

    while (number_of_unvisited_clients > 0) {
        //go through all known nodes
        //find the edge with smallest weight
        //if that edge was found from start node
            //current_first_step is the to address of that edge
        //find_and_remove that client
        //add taht client to visited
        //update_shortest_distance_from that client
    }

}

