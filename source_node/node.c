
#include "../source_shared/protocol.h"
#include "../print_lib/print_lib.h"
#include <stdbool.h>
#include <netinet/in.h>
#include <stdio.h>
#include "create_sockets.h"

int baseport = 0;
int own_address;

struct node *node;
struct edge *edges;
struct table *table;

int number_of_table_entries;

struct sockaddr_in udp_sockaddr;

int quit = 0;
int keep_going = 1;

char *quit_char = "QUIT";

/*
Construct message to send to routing_server.
buffer_to_send is malloced already.
*/
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

/*
Sending information to routing_server. For explanation about protocol - see README
*/
ssize_t send_information(int client_socket) {
    ssize_t bytes = 0;
    ssize_t size_of_message = (sizeof(int) * 2) + (sizeof(struct edge) * node->number_of_edges);

    char *buffer_to_send = malloc(size_of_message);
    assert(buffer_to_send);

    construct_message(buffer_to_send);
    
    bytes = send_message(client_socket, buffer_to_send, size_of_message);
    if (bytes == -1) {
        free(buffer_to_send);
        return EXIT_FAILURE;
    }
    if (bytes == 0) {
        fprintf(stdout, "Server has disconnected\n");
        free(buffer_to_send);
        exit(EXIT_FAILURE); //No point in continuing with this node if server has disconnected.
    }

    free(buffer_to_send);
    return bytes;
}

/*
Param info is a char pointer containing address and weight of edge.
*/
struct edge create_edge(char *info) {
    struct edge new_edge;

    char *to_address = strtok(info, ":");
    char *weight = strtok(NULL, "");

    new_edge.to_address = atoi(to_address);
    
    new_edge.weight = atoi(weight);

    return new_edge;
}

/*
Creating node struct and filling it with information about own_address, socket, number of edges and the actual edges (these are edge structs).
*/
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
        struct edge new_edge = create_edge(info_edges[i]);
        new_edge.from_address = node->own_address;
        edges[i] = new_edge;
    }
}

/*
Freeing all global variables.
*/
void free_all() {
    free(edges);
    free(node);
    free(table);
}

/*
Receiving routing table from routing_server.
*/
void receive_table(int client_socket) {
    receive_message(client_socket, &number_of_table_entries, sizeof(int));

    table = calloc(number_of_table_entries, sizeof(struct table));

    ssize_t size_of_rest_of_message = number_of_table_entries * sizeof(struct table);

    receive_message(client_socket, table, size_of_rest_of_message); 
}

/*
Printing all info about node struct.
*/
void print_node() {
    fprintf(stdout, "NODE:\n");

    fprintf(stdout, "Own address: %d\tClient socket: %d\tNumber of edges: %d\n\n", node->own_address, node->client_socket, node->number_of_edges);

    fprintf(stdout, "EDGES: (including invalid edges that routing server will delete)\n");
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


/*
Looking through routing table, returning address of first client on route to destination_address.
If no address is found, -1 is returned. This means that this client is not really on route from node 1 to destination_address, and packet has arrived at wrong client.
*/
int find_next_client(int destination_address) {
    for (int i = 0; i < number_of_table_entries; i++) {
        struct table current_table = table[i];
        if (current_table.to_address == destination_address) {
            return current_table.first_client_on_route;
        }
    }
    return -1;
}

/*
Unpacking message header received via UDP from another client.
*/
void unpack_message_header(unsigned short *to_address, unsigned short *packet_length, char *message) {
    int offset_in_message = 0;

    memcpy(packet_length, &(message[offset_in_message]), sizeof(unsigned short));
    offset_in_message += sizeof(unsigned short);

    memcpy(to_address, &(message[offset_in_message]), sizeof(unsigned short));
}

/*
Making packet to send via UDP (function used only be node 1)
*/
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

/*
Send message to other client via UDP.
*/
void send_message_udp(int udp_socket, unsigned short to_address, unsigned short packet_length, char *message) {
    struct sockaddr_in send_to_sockaddr;

    send_to_sockaddr.sin_family = AF_INET; 
    send_to_sockaddr.sin_port = htons(baseport + to_address);
    send_to_sockaddr.sin_addr.s_addr = INADDR_ANY;

    sendto(udp_socket, message, packet_length, 0, (struct sockaddr*)&send_to_sockaddr, sizeof(send_to_sockaddr));
}

/*
Splitting one line from file (consisting of address of destination client and message), putting first psrt (address) into to_address and returning rest.
*/
char *split_message_from_file(unsigned short *to_address, char *line_from_file) {
    char *temp_to_address = strtok(line_from_file, " ");
    char *rest_of_message = strtok(NULL, "\n");
    *to_address = (unsigned short) atoi(temp_to_address);
    return rest_of_message;
}

/*
Node with own_address = 1 runs this function.
Line is read from file
Line is splitted
Check to see if destination address == own_address
Calculating length of packet (packet_length)
Making actual packet
Finding first client on route to destination address (checking routing table)
Sending packet
*/
void send_messages_start_node(int udp_socket) {
    FILE *file_pointer = fopen("data.txt", "r");

    char buffer[1024];

    while (fgets(buffer, 1000, file_pointer) != NULL) {
        unsigned short packet_length;
        unsigned short to_address = 0;

        char *message = split_message_from_file(&to_address, buffer);

        if ((int) to_address == own_address) {
            if (strcmp(message, quit_char) == 0) {
                fclose(file_pointer);
                return;
            }
        }

        packet_length = (unsigned short) ((sizeof(unsigned short) * 3) + strlen(message) + 1);

        char *packet = malloc(packet_length * sizeof(char));
        make_packet(packet, htons(packet_length), htons(to_address), htons(own_address), message);
        packet[packet_length-1] = '\0';

        int address_next_client = find_next_client((int)to_address);

        if (address_next_client == -1) {
            printf("Could not find client with address %hu!\n", to_address);
            free(packet);
            continue;
        }

        print_pkt((unsigned char*) packet);
        printf("Sending message to address %hu: %s\n", to_address, message);
        send_message_udp(udp_socket, address_next_client, packet_length, packet);

        free(packet);
    }
    fclose(file_pointer);
}

/*
Packet has arrived at destination.
*/
int packet_arrived(char *message) {
    print_received_pkt((unsigned short) own_address, (unsigned char*) message);

    int length_of_header = sizeof(unsigned short) * 3;
    char *actual_message = message + length_of_header;

    printf("Received message: %s\n", actual_message);
    if (strcmp(actual_message, quit_char) == 0) {
        return quit;
    }
    return keep_going;
}

/*
Forwarding message to next client
*/
void forward_message(int udp_socket, char *message, unsigned short to_address, unsigned short packet_length) {
    print_forwarded_pkt((unsigned short)own_address, (unsigned char*) message);

    int address_next_client = find_next_client((int) to_address);
    if (address_next_client == -1) {
        printf("Could not find next client! \n");
        return;
    }

    send_message_udp(udp_socket, (unsigned short) address_next_client, packet_length, message);
}

/*
All clients other than client 1 runs this function.
Receiving message into char pinter message (parameter) (taken in as parameter because it is malloced on the outside)
Making sure it is null terminated.
Unpacking the header (only need packet length and destination address at this point)

If the packet has arrived at destination -> packet_arrived
Else -> find first client on route to destination -> forward_message
*/
int receive_message_udp(int udp_socket, char *message, int max_size_message) {
    struct sockaddr_in src;
    socklen_t src_len = sizeof(src);

    unsigned short to_address = 0;
    unsigned short packet_length = 0;
    ssize_t rec = 0;

    rec = recvfrom(udp_socket, message, max_size_message, 0, (struct sockaddr*)&src, &src_len);
    
    message[rec-1] = '\0';

    unpack_message_header(&to_address, &packet_length, message);
    to_address = ntohs(to_address);
    packet_length = ntohs(packet_length);

    if (rec != (ssize_t) packet_length) {
        printf("Only %zu out of %hu bytes received.\n", rec, packet_length);
        return keep_going;
    }
    
    if ((int)to_address == own_address) {
        if (packet_arrived(message) == quit){
            return quit;
        }
    }
    else {
        forward_message(udp_socket, message, to_address, packet_length);
    }
    return keep_going;
}

/*
While client has not received packet containing "EXIT", client continues to receive messages.
*/
void receive_messages_nodes(int udp_socket) {
    size_t max_size_message = 1024;

    char *message;
    int keep_looping = keep_going;
    while (keep_looping != quit) {
        message = malloc(max_size_message);
        assert(message);

        keep_looping = receive_message_udp(udp_socket, message, max_size_message);
        free(message);
    }
}

void run_udp_communication(int udp_socket) {
    if (own_address == 1) {
        sleep(1);
        send_messages_start_node(udp_socket);
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

    int client_socket = create_and_connect_socket(baseport);
    if (client_socket == EXIT_FAILURE) {
        exit(-2);
    }

    hand_shake(client_socket);

    int number_of_edges = argc-3;
    char **edges_info = argv + 3;
    own_address = atoi(argv[2]);

    create_node_struct(own_address, edges_info, client_socket, number_of_edges);

    send_information(client_socket);

    receive_table(client_socket);
    close(client_socket);

    print_node();

    int udp_socket = create_and_connect_udp_socket(baseport, own_address);
    if (udp_socket == EXIT_FAILURE) {
        free_all();
        exit(-1);
    }
    
    run_udp_communication(udp_socket);    
    close(udp_socket);
    free_all();
    return EXIT_SUCCESS; 
} 
