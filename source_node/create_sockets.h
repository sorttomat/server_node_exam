#ifndef __CREATE_SOCKETS_H
#define __CREATE_SOCKETS_H

int create_and_connect_socket(int baseport);
int create_and_connect_udp_socket(int baseport, int own_address);
int hand_shake(int client_socket);

#endif
