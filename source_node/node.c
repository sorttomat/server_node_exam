
#include "../source_shared/protocol.h"
#include "../print_lib/print_lib.h"
#include <stdbool.h>
#include <netinet/in.h>
#include <stdio.h>

int baseport = 0;
int own_address;

struct node *node;
struct edge *edges;
struct table *table;

int number_of_table_entries;

struct sockaddr_in udp_sockaddr;

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

int create_and_connect_udp_socket() {
    struct sockaddr_in udp_sockaddr;
	int ret;
    int yes = -1; 
    int udp_socket;

	udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (udp_socket == -1) {
		perror("socket");
        exit(EXIT_FAILURE);
	}

	udp_sockaddr.sin_family = AF_INET; 
	udp_sockaddr.sin_port = htons(baseport + own_address);	
    udp_sockaddr.sin_addr.s_addr = INADDR_ANY;

	setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (own_address == 1) {
        sleep(1);
    }

	ret = bind(udp_socket, (struct sockaddr*)&udp_sockaddr, sizeof(udp_sockaddr));
	if (ret) {
		perror("bind");
        exit(EXIT_FAILURE);
	}

    return udp_socket;
}

int find_next_client(int destination_address) {
    for (int i = 0; i < number_of_table_entries; i++) {
        struct table current_table = table[i];
        if (current_table.to_address == destination_address) {
            return current_table.first_client_on_route;
        }
    }
    return -1;
}

void unpack_message_header(unsigned short *to_address, unsigned short *from_address, unsigned short *packet_length, char *message) {
    int offset_in_message = 0;

    memcpy(packet_length, &(message[offset_in_message]), sizeof(unsigned short));
    offset_in_message += sizeof(unsigned short);

    memcpy(to_address, &(message[offset_in_message]), sizeof(unsigned short));
    offset_in_message += sizeof(unsigned short);

    memcpy(from_address, &(message[offset_in_message]), sizeof(unsigned short));
    offset_in_message += sizeof(unsigned short);
}

void make_packet(char *buffer, unsigned short packet_length, unsigned short to_address, unsigned short from_address, char *message) {

    int offset_buffer = 0;

    memcpy(&(buffer[offset_buffer]), &packet_length, sizeof(unsigned short));
    offset_buffer += sizeof(unsigned short);

    memcpy(&(buffer[offset_buffer]), &to_address, sizeof(unsigned short));
    offset_buffer += sizeof(unsigned short);

    memcpy(&(buffer[offset_buffer]), &from_address, sizeof(unsigned short));
    offset_buffer += sizeof(unsigned short);

    memcpy(&(buffer[offset_buffer]), message, sizeof(char) * strlen(message));

}

void send_message_udp(int udp_socket, unsigned short to_address, char *message) {
    struct sockaddr_in send_to_sockaddr;

    send_to_sockaddr.sin_family = AF_INET; 
    send_to_sockaddr.sin_port = htons(baseport + to_address);
    send_to_sockaddr.sin_addr.s_addr = INADDR_ANY;

    sendto(udp_socket, message, 1000, 0, (struct sockaddr*)&send_to_sockaddr, sizeof(send_to_sockaddr));
}

char *split_message_from_file(unsigned short *to_address, char *line_from_file) {
    char *temp_to_address = strtok(line_from_file, " ");
    char *rest_of_message = strtok(NULL, "\n");
    *to_address = (unsigned short) atoi(temp_to_address);
    return rest_of_message;
}

void send_messages_start_node(int udp_socket) {
    FILE *file_pointer = fopen("data.txt", "r");

    char buffer[1024];

    while (fgets(buffer, 1000, file_pointer) != NULL) {
        unsigned short packet_length = 0;
        unsigned short to_address = 0;

        char *message = split_message_from_file(&to_address, buffer);

        packet_length = (unsigned short) ((sizeof(unsigned short) * 3) + strlen(message) + 1);

        char *packet = malloc(packet_length * sizeof(char));
        make_packet(packet, htons(packet_length), htons(to_address), htons(own_address), message);
        packet[packet_length-1] = '\0';

        int address_next_client = find_next_client((int)to_address);

        if (address_next_client == -1) {
            printf("Could not find client with address %hu! :(\n", to_address);
            free(packet);
            continue;
        }

        printf("%d MESSAGE: %s\n", to_address, message);

        print_pkt((unsigned char*) packet);
        send_message_udp(udp_socket, address_next_client, packet);

        free(packet);
    }
    fclose(file_pointer);
}

void receive_messages_nodes(int udp_socket) {
    struct sockaddr_in src;
    socklen_t src_len = sizeof(src);

    unsigned short to_address = 0;
    unsigned short from_address = 0;
    unsigned short packet_length = 0;
    size_t max_size_message = 1024;

    char *message;
    ssize_t rec;
    while (1) {
        printf("Receiving ....\n");
        message = malloc(max_size_message);
        rec = recvfrom(udp_socket, message, max_size_message, 0, (struct sockaddr*)&src, &src_len);
        printf("Received\n");
        
        message[rec-1] = '\0';
        unpack_message_header(&to_address, &from_address, &packet_length, message);
        to_address = ntohs(to_address);
        from_address = ntohs(from_address);
        packet_length = ntohs(packet_length);
        
        if ((int)to_address == own_address) {
            print_received_pkt((unsigned short) own_address, (unsigned char*) message);
            //packet has reached destination
            int length_of_header = sizeof(unsigned short) * 3;
            char *actual_message = message + length_of_header;
            printf("RECEIVED MESSAGE: %lu [%s]\n", strlen(actual_message), actual_message);

            if (strncmp(actual_message, "QUIT", 4) == 0) {
                printf("QUITTING!");
                close(udp_socket);
                free(message);
                exit(EXIT_SUCCESS);
            }
        }
        else {
            print_forwarded_pkt((unsigned short)own_address, (unsigned char*) message);
            int address_next_client = find_next_client((int) to_address);
            printf("FORWARDING TO NEXT CLIENT: %d\n", address_next_client);
            if (address_next_client == -1) {
                printf("Could not find next client! :(\n");
                free(message);
                continue;
            }
            send_message_udp(udp_socket, (unsigned short) address_next_client, message);
        }
        free(message); //could lead to problems?
    }
}

void run_udp_communication(int udp_socket) {
    if (own_address == 1) {
        sleep(1);
        printf("Sending message!\n");
        send_messages_start_node(udp_socket);
        //close(udp_socket);
    }

    else {
        receive_messages_nodes(udp_socket);
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
    own_address = atoi(argv[2]);

    create_node_struct(own_address, edges_info, client_socket, number_of_edges);

    send_information(client_socket);

    receive_table(client_socket);

    print_node();

    //bind to own socket
    int udp_socket = create_and_connect_udp_socket();
    printf("Node %d connected to socket with socket %d!\n", own_address, udp_socket);
    //run 
    run_udp_communication(udp_socket);
    free_all();
    return EXIT_SUCCESS; 
} 