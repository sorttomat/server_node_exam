struct edge {
    int from_address;
    int to_address;
    int weight;
};

struct node {
    int own_address;
    int client_socket;
    struct edge *edges;
    int number_of_edges;
};