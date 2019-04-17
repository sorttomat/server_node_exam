
#include "../source_shared/protocol.h"

int baseport = 0;

struct node *node;
struct edge *edges;

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

    printf("Connecting to router ..........\n");
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
        printf("Something wrong with receive message\n");
        return EXIT_FAILURE;
    }
    if (bytes == 0) {
        printf("Server has disconnected\n");
        return EXIT_SUCCESS; //?
    }

    buf[bytes] = '\0';
    printf("%s\n", buf);
    return EXIT_SUCCESS;
}

void deconstruct_message(char *message) { //just for testing
    int own_address = 0;
    int number_of_edges = 0;
    struct edge edge;

    memcpy(&own_address, &(message[0]), sizeof(int));
    memcpy(&(number_of_edges), &(message[4]), sizeof(int));
    memcpy(&(edge), &(message[8]), sizeof(struct edge));

    printf("Own address: %d\nNumber of edges: %d\nTo: %d\nFrom: %d\nWeight: %d\n\n", own_address, number_of_edges, edge.to_address, edge.from_address, edge.weight);
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
    //bytes = send(client_socket, buffer_to_send, size_of_message, 0);

    bytes = send_message(client_socket, &size_of_message, sizeof(int));
    bytes = send_message(client_socket, buffer_to_send, size_of_message);
    return bytes;
}

size_t receive_all_nodes_connected(int client_socket) {
    size_t bytes = 0;

    char buffer[strlen(ALL_CLIENTS_CONNECTED)];
    bytes = receive_message(client_socket, buffer, strlen(ALL_CLIENTS_CONNECTED));

    if (bytes == -1) {
        return EXIT_FAILURE;
    }
    if (bytes == 0) {
        printf("Server has disconnected\n");
        return EXIT_SUCCESS; //?
    }

    printf("%s\n", buffer);

    close(client_socket);
    return EXIT_SUCCESS;
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

void create_node_struct(char *own_address, char *info_edges[], int client_socket, int number_of_edges) {
    printf("info_edges[0] %s\n", info_edges[0]);
    node = malloc(sizeof(struct node));
    edges = calloc(number_of_edges, sizeof(struct edge));
    printf("Create_node_struct\n");

    if (node == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    node->client_socket = client_socket;
    node->own_address = atoi(own_address);
    node->edges = edges;
    node->number_of_edges = number_of_edges;

    for (int i = 0; i < number_of_edges; i++) {
        struct edge new_edge = create_edge(info_edges[i], strlen(info_edges[i]));
        new_edge.from_address = node->own_address;
        edges[i] = new_edge;
    }
}

int main(int argc, char *argv[]) {
    if (argc <= 2) {
        printf("Number of arguments are too few!\n");
        exit(EXIT_FAILURE);
    }

    baseport = atoi(argv[1]);
    int client_socket = create_and_connect_socket();
    if (client_socket == EXIT_FAILURE) {
        exit(EXIT_FAILURE);
    }

    hand_shake(client_socket);
    printf("Here\n");

    int number_of_edges_argc = argc-3;

    // char **edges_info = &(argv[3]);
    char **edges_info = argv + 3;


    printf("argv[3] %s\n", argv[3]);

    create_node_struct(argv[2], edges_info, client_socket, number_of_edges_argc);
    
    //send_and_receive(client_socket);
    
    printf("%d\n", node->number_of_edges);
    for (int i = 0; i < node->number_of_edges; i++) {
        printf("to address: %d\nweight: %d\n", node->edges[i].to_address, node->edges[i].weight);
    }

    send_information(client_socket);
    receive_all_nodes_connected(client_socket);
    return EXIT_SUCCESS; 
} 