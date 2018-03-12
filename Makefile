all: server client

server: packet.c packet.h
	gcc server.c packet.c -o server

client: packet.c packet.h
	gcc client.c packet.c -o client

clean:
	rm -f server client
