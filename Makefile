CC = gcc
CCFLAGS= -g -Wall -Wextra -Wpedantic -std=c99

all: clean routing_server node 

routing_server: source_router/*.c source_shared/*.c print_lib/*.c
	$(CC) $(CCFLAGS) $^ -o $@

node: source_node/*.c source_shared/*.c print_lib/*.c
	$(CC) $(CCFLAGS) $^ -o $@

run: all
	bash run_1.sh 6000
	bash run_2.sh 7000

clean:
	rm -f routing_server
	rm -f node
	
