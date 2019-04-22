
#include "../source_shared/protocol.h"
#include "../print_lib/print_lib.h"

struct table {
    int to_address;
    int first_client_on_route;
};

int baseport = 0;

struct node *node;
struct edge *edges;
struct table *table;

int number_of_table_entries;

int create_and_connect_socket() {
    struct sockaddr_in server_addr;

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(baseport);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    fprintf(stdout, "Connecting to router ..........\n");
    int ret = connect(client_socket, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
    if (ret == -1) {
        perror("connect");
        return EXIT_FAILURE;
    }
    return client_socket;
}

int hand_shake(int client_socket) {
    char buf[strlen(RESPONSE_SUCCESS)];

    size_t bytes = receive_message(client_socket, buf, strlen(RESPONSE_SUCCESS)); //could be response_failure
    if (bytes == -1) {
        fprintf(stdout, "Something wrong with receive message\n");
        return EXIT_FAILURE;
    }
    if (bytes == 0) {
        fprintf(stdout, "Server has disconnected\n");
        return EXIT_SUCCESS; //?
    }

    buf[bytes] = '\0';
    fprintf(stdout, "%s\n", buf);
    return EXIT_SUCCESS;
}

void construct_message(char *buffer_to_send) {
    construct_header(buffer_to_send, *node);

    int start_of_edges_in_buffer = sizeof(int) * 2;

    int edge_counter = 0;
    int place_edge_in_buffer = start_of_edges_in_buffer;

    while (edge_counter < node->number_of_edges) {
        memcpy(&(buffer_to_send[place_edge_in_buffer]), &(node->edges[edge_counter]), sizeof(struct edge));
        edge_counter++;
        place_edge_in_buffer += sizeof(struct edge);
    }
}

size_t send_information(int client_socket) {
    size_t bytes = 0;

    char *buffer_to_send = malloc((sizeof(int) * 2) + (sizeof(struct edge) * node->number_of_edges));
    assert(buffer_to_send);

    construct_message(buffer_to_send);
    
    size_t size_of_message = (sizeof(int) * 2) + (sizeof(struct edge) * node->number_of_edges);

    bytes = send_message(client_socket, buffer_to_send, size_of_message);
    if (bytes == -1) {
        free(buffer_to_send);
        return EXIT_FAILURE;
    }
    if (bytes == 0) {
        fprintf(stdout, "Server has disconnected\n");
        free(buffer_to_send);
        return EXIT_SUCCESS; //?
    }

    free(buffer_to_send);
    return bytes;
}

struct edge create_edge(char *info, int size_of_info) {
    struct edge new_edge;
    char buf[size_of_info];
    int size_weight;

    int i = 0;
    while (info[i] != ':') {
        buf[i] = info[i];
        i++;
    }
    buf[i] = '\0';

    new_edge.to_address = atoi(buf);

    size_weight = size_of_info-(i);
    
    char weight[size_weight];
    memcpy(&weight, &(info[i+1]), size_weight);
    
    new_edge.weight = atoi(weight);

    return new_edge;
}

void create_node_struct(int own_address, char *info_edges[], int client_socket, int number_of_edges) {

    node = malloc(sizeof(struct node));
    if (node == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    edges = calloc(number_of_edges, sizeof(struct edge));
    if (edges == NULL) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    node->client_socket = client_socket;
    node->own_address = own_address;
    node->edges = edges;
    node->number_of_edges = number_of_edges;

    for (int i = 0; i < number_of_edges; i++) {
        struct edge new_edge = create_edge(info_edges[i], strlen(info_edges[i]));
        new_edge.from_address = node->own_address;
        edges[i] = new_edge;
    }
}

void free_all() {
    free(edges);
    free(node);
    free(table);
}

void receive_table(int client_socket) {
    receive_message(client_socket, &number_of_table_entries, sizeof(int));

    table = calloc(number_of_table_entries, sizeof(struct table));

    size_t size_of_rest_of_message = number_of_table_entries * sizeof(struct table);

    receive_message(client_socket, table, size_of_rest_of_message);
    
}

void print_node() {
    fprintf(stdout, "NODE:\n");

    fprintf(stdout, "Own address: %d\tClient socket: %d\tNumber of edges: %d\n\n", node->own_address, node->client_socket, node->number_of_edges);

    fprintf(stdout, "EDGES:\n");
    for (int i = 0; i < node->number_of_edges; i++) {
        fprintf(stdout, "To: %d\tFrom: %d\tWeight: %d\n", node->edges[i].to_address, node->edges[i].from_address, node->edges[i].weight);
    }
    fprintf(stdout, "\n\n");
    fprintf(stdout, "TABLE:\n");

    for (int i = 0; i < number_of_table_entries; i++) {
        struct table entry = table[i];
        fprintf(stdout, "Address to: %d\tFirst client on route: %d\n", entry.to_address, entry.first_client_on_route);
        fprintf(stdout, "\n");
    }

}

int main(int argc, char *argv[]) {
    if (argc <= 2) {
        fprintf(stdout, "Number of arguments are too few!\n");
        exit(EXIT_FAILURE);
    }

    baseport = atoi(argv[1]);
    int client_socket = create_and_connect_socket();
    if (client_socket == EXIT_FAILURE) {
        exit(EXIT_FAILURE);
    }

    hand_shake(client_socket);

    int number_of_edges = argc-3;
    char **edges_info = argv + 3;
    int own_address = atoi(argv[2]);

    create_node_struct(own_address, edges_info, client_socket, number_of_edges);

    send_information(client_socket);

    receive_table(client_socket);

    print_node();

    free_all();
    return EXIT_SUCCESS; 
} 