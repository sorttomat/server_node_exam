
#include "../source_shared/protocol.h"

int baseport = 0;

struct node *node;

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

    ssize_t bytes = receive_message(client_socket, buf, strlen(RESPONSE_SUCCESS)); //could be response_failure
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

ssize_t send_information(int client_socket) {
    ssize_t bytes;
    char send_text[255];

    fgets(send_text, 255, stdin); //For morroskyld
    ssize_t number_of_bytes = strlen(send_text);

    bytes = send_message(client_socket, &number_of_bytes, sizeof(int));
    if (bytes == -1) {
        printf("Error with send_message\n");
        return EXIT_FAILURE;
    }
    if (bytes == 0) {
        printf("Disconnection\n");
        return EXIT_FAILURE;
    }

    bytes = send_message(client_socket, send_text, number_of_bytes);
    if (bytes == -1) {
        printf("Error with send_message\n");
        return EXIT_FAILURE;
    }
    if (bytes == 0) {
        printf("Disconnection\n");
        return EXIT_FAILURE;
    }
    return bytes;
}

int send_and_receive(int client_socket) {
    ssize_t bytes;

    bytes = send_information(client_socket);

    char buffer[strlen(ALL_CLIENTS_CONNECTED)];
    bytes = recv(client_socket, buffer, strlen(ALL_CLIENTS_CONNECTED), 0);
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
    node = malloc(sizeof(struct node));
    if (node == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    node->client_socket = client_socket;
    node->own_address = atoi(own_address);

    for (int i = 3; i < number_of_edges; i++) {
        struct edge new_edge = create_edge(info_edges[i], strlen(info_edges[i]));
        new_edge.from_address = node->own_address;
    }
}

int main(int argc, char *argv[]) {
    // if (argc <= 2) {
    //     printf("Number of arguments are too few!\n");
    //     exit(EXIT_FAILURE);
    // }

    baseport = atoi(argv[1]);
    int client_socket = create_and_connect_socket();
    if (client_socket == EXIT_FAILURE) {
        exit(EXIT_FAILURE);
    }

    hand_shake(client_socket);


    //create_node_struct(argv[2], argv, client_socket, argc-2);
    send_and_receive(client_socket);

    return EXIT_SUCCESS; 
} 