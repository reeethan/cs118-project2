all: server client purge

server: server.c packet.c packet.h
	gcc server.c packet.c -o server

client: client.c packet.c packet.h
	gcc client.c packet.c -o client

purge: 
	rm -f received.data

clean:
	rm -f server client
