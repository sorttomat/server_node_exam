#include <netinet/in.h>
#include "../source_shared/protocol.h"
#include "create_sockets.h"

/*
Connect to socket (TCP)
Used by nodes
*/
int create_and_connect_socket(int baseport) {
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
        exit(-2);
    }
    return client_socket;
}

/*
Node receiving either response failure or response success.
*/
int hand_shake(int client_socket) {
    char buf[strlen(RESPONSE_SUCCESS)];

    ssize_t bytes = receive_message(client_socket, buf, strlen(RESPONSE_SUCCESS)); //could be response_failure too
    if (bytes == -1) {
        fprintf(stdout, "Something wrong with receive message\n");
        return EXIT_FAILURE;
    }
    if (bytes == 0) {
        fprintf(stdout, "Server has disconnected\n");
        exit(EXIT_FAILURE); //No point to keep going if server has disconnected
    }

    buf[bytes] = '\0';
    fprintf(stdout, "%s\n", buf);
    return EXIT_SUCCESS;
}

/*
Creating and binding to socket (UDP)
Used by nodes
*/
int create_and_connect_udp_socket(int baseport, int own_address) {
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

    if (baseport + own_address > 65500) {
        printf("Invalid own address\n");
        return EXIT_FAILURE;
    }

	ret = bind(udp_socket, (struct sockaddr*)&udp_sockaddr, sizeof(udp_sockaddr));
	if (ret) {
		perror("bind");
        close(udp_socket);
        return EXIT_FAILURE;
	}

    return udp_socket;
}
