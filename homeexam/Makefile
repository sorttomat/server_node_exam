CCFLAGS= -g 

all: routing_server node

routing_server: <Her skal alle c-filer som routing_server avhenger av>
	gcc <Her skal alle c-filer som routing_server avhenger av> $(CCFLAGS) -o routing_server

node: <Her skal alle c-filer som node avhenger av>
	gcc <Her skal alle c-filer som node avhenger av> $(CCFLAGS) -o node


run: all
	bash run.sh

clean:
	rm -f routing_server
	rm -f node
