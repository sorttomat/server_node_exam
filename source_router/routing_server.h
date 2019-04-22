#ifndef __ROUTING_H
#define __ROUTING_H

struct table {
    int to_address;
    int weight;
    int first_client_on_route;
};

#endif
