#include "dijkstra.h"
#include "client.h"
#include "../source_shared/protocol.h"

#include <stdbool.h>
#include <stdlib.h>

/*
Make array of dijkstra_node structs.
Each dijkstra_node contains (amongst other things) a client struct from global array of clients (clients)
*/
void make_array_for_dijkstra() {
    for (int i = 0; i < max_num_clients; i++) {
        struct client current_client = clients[i];

        array_of_dijkstra_nodes[i].client = current_client;
        array_of_dijkstra_nodes[i].visited = 0;
        array_of_dijkstra_nodes[i].weight = -1;
        array_of_dijkstra_nodes[i].path = malloc(sizeof(int));
        array_of_dijkstra_nodes[i].number_of_nodes_in_path = 0;
    }
}

/*
Go through all dijkstra_node structs to find the correct one.
*/
struct dijkstra_node *find_dijkstra_node(int address) {
    for (int i = 0; i < max_num_clients; i++) {
        struct dijkstra_node *current = &(array_of_dijkstra_nodes[i]);
        if (current->client.own_address == address) {
            return current;
        }
    }
    return NULL;
}

/*
Function that updates weight and current shortest path (int array) to node_to_update.
Updating only if node_to_update is not yet visited.
*/
void update_weight_and_path_of_node(struct dijkstra_node *node_to_update, struct edge edge, int current_weight, int *path, int path_length) {
    if (node_to_update->visited == 0) {
        int new_weight = current_weight + edge.weight;

        if (node_to_update->weight == -1 || new_weight < node_to_update->weight) {
            free(node_to_update->path);

            node_to_update->path = calloc(path_length + 1, sizeof(int));

            int i;
            for (i = 0; i < path_length; i++) {
                node_to_update->path[i] = path[i];
            }
            node_to_update->path[i] = node_to_update->client.own_address;

            node_to_update->number_of_nodes_in_path = path_length+1;
            node_to_update->weight = new_weight;
        }
    }
}

/*
Update neighbor nodes of current_node.
I use the edge structs from current_node to find each neighbor.
*/
void update_neighbor_nodes(struct dijkstra_node current_node) {
    int current_weight = current_node.weight;
    int *current_path = current_node.path; //this is pointer, must be copied when used
    int current_path_length = current_node.number_of_nodes_in_path;

    int number_of_edges = current_node.client.number_of_edges;
    for (int i = 0; i < number_of_edges; i++) {
        struct edge current_edge = current_node.client.edges[i];

        struct dijkstra_node *current = find_dijkstra_node(current_edge.to_address);

        if (current == NULL) { //Unlikely!
            fprintf(stdout, "No node found with address %d\n", current_edge.to_address);
            return;
        }
        
        update_weight_and_path_of_node(current, current_edge, current_weight, current_path, current_path_length);
    }
}

/*
Find the next noe to visit.
Find the edge with the smallest weight (looking through all visited nodes), returning the node in the other end of said edge.
*/
struct dijkstra_node *find_next_node() {
    int A_REALLY_BIG_NUMBER = 100000;

    struct dijkstra_node *next_node = NULL;
    int smallest_weight = A_REALLY_BIG_NUMBER;

    for (int i = 0; i < max_num_clients; i++) {
        struct dijkstra_node *current_node = &(array_of_dijkstra_nodes[i]);
        if (current_node->visited == 1) {
            for (int j = 0; j < current_node->client.number_of_edges; j++) {
                struct edge current_edge = current_node->client.edges[j];

                if (current_edge.weight < smallest_weight) {
                    struct dijkstra_node *temp_next_node = find_dijkstra_node(current_edge.to_address);
                    if (temp_next_node->visited == 0) {
                        next_node = temp_next_node;
                        smallest_weight = current_edge.weight;
                    }
                }
            }
        }
    }
    return next_node;
}

/*
Marking node as visited.
*/
void visit_node(struct dijkstra_node *node_to_visit) {
    node_to_visit->visited = 1;
}

/*
Checking if an address is on a (shortest) path.
*/
bool is_on_path(int address, int path[], int path_length) {
    for (int i = 0; i < path_length; i++) {
        if (path[i] == address) {
            return true;
        }
    }
    return false;
}

/*
Running dijkstra.
1. Creating array of dijkstra nodes
2. Finding start node, visit start node and updating start nodes neighbors 
3. While not yet all clients have been visited, visit one by one and update nodes as I go.
4. For every node, shortest path from node 1 to that node is printed out (can be seen in routing_server_log_txt)
*/
void dijkstra() {
    make_array_for_dijkstra();

    int number_of_unvisited_nodes = max_num_clients;
    int number_of_visited_nodes = 0;

    int start_address = 1;

    struct dijkstra_node *start_node = find_dijkstra_node(start_address);

    visit_node(start_node);
    start_node->weight = 0;
    start_node->path[0] = start_node->client.own_address;
    start_node->number_of_nodes_in_path = 1;

    update_neighbor_nodes(*start_node);
    number_of_visited_nodes++;
    number_of_unvisited_nodes--;

    struct dijkstra_node *next_node;
    while (number_of_visited_nodes < max_num_clients) {
        next_node = find_next_node();
        visit_node(next_node);
        update_neighbor_nodes(*next_node);
        number_of_unvisited_nodes--;
        number_of_visited_nodes++;
    }

    for (int i = 0; i< max_num_clients; i++) {
        struct dijkstra_node current = array_of_dijkstra_nodes[i];
        printf("Shortest path for node %d\n", current.client.own_address);
        for (int j = 0; j < current.number_of_nodes_in_path; j++) {
            printf("%d -> ", current.path[j]);
        }
        printf("\n\n");
    }
}
