#ifndef __DIJKSTRA_H
#define __DIJKSTRA_H

#include "client.h"
#include <stdbool.h>

struct dijkstra_node {
    struct client client;
    int weight;
    char visited;
    int *path;
    int number_of_nodes_in_path;
};

struct dijkstra_node *array_of_dijkstra_nodes;

void new_dijkstra();
bool is_on_path(int address, int path[], int path_length);
#endif

