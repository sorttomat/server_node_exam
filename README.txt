To run:
> make run

Baseport is written in makefile, can be changed there.

Protocol to send packets from nodes to routing server:
own address (1 int)
number of edges (1 int)
number of edges * edge structs (x * sizeof(edge struct))

Both node.c and routing_server.c has access to edge structs, so they both know how big they are.

Protocol to send routing table from routing_server to node:
number of entries (1 int)
number of entries * {to address (1 int)
                    first client in route (1 int)}


