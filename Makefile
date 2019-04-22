CC = gcc
CCFLAGS= -g -std=c99

all: clean routing_server node #dijkstra

routing_server: source_router/routing_server.c source_shared/protocol.c print_lib/print_lib.c
	$(CC) $(CCFLAGS) $^ -o $@

node: source_node/node.c source_shared/protocol.c print_lib/print_lib.c
	$(CC) $(CCFLAGS) $^ -o $@

run: all
	bash run.sh

clean:
	rm -f routing_server
	rm -f node
	rm -f protocol
	
