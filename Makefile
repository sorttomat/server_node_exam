CCFLAGS= -g 

all: routing_server node

routing_server: 
	gcc source_router/routing_server.c $(CCFLAGS) -o routing_server

node: 
	gcc source_node/node.c $(CCFLAGS) -o node


run: all
	bash run.sh

clean:
	rm -f routing_server
	rm -f node
