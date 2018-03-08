all: server client

server: packet.c packet.h
client: packet.c packet.h

clean:
	rm -f server client
