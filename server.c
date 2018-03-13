#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "packet.h"

void error(char *msg)
{
    perror(msg);
    exit(1);
}

// Send packet to socket and print formatted output
void send_packet(struct packet *p, int fd, struct sockaddr *addr)
{
    int n = sendto(fd, p, PACKET_SIZE, 0, addr, sizeof(struct sockaddr));
    if (n < 0)
        error("sendto");

    if(n > 0) {
    	printf("Sending packet %d", p->seq_num);
    	if (HAS_FLAG(p, SYN))
        	printf(" SYN");
    	if (HAS_FLAG(p, ACK))
			printf(" ACK");
    	if (HAS_FLAG(p, FIN))
        	printf(" FIN");
    	printf("\n");
    }
}

// Receive packet from socket and print formatted output
int recv_packet(struct packet *p, int fd, struct sockaddr *addr, socklen_t *len)
{
    int n = recvfrom(fd, p, PACKET_SIZE, 0, addr, len);
    if (n < 0)
        error("recvfrom");
    if (n > 0)
    	printf("Receiving packet %d\n", p->ack_num);

    return n;
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    socklen_t addr_len = sizeof(serv_addr);

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // create socket
    if (sockfd < 0)
        error("ERROR opening socket");
    memset((char *) &serv_addr, 0, sizeof(serv_addr)); // reset memory

    // fill in address info
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    struct packet pkt_out, pkt_in;
    while(1) {
        // Wait to receive packet with SYN flag
        n = recv_packet(&pkt_in, sockfd, (struct sockaddr *) &serv_addr, &addr_len);
        if(n > 0 && HAS_FLAG(&pkt_in, SYN)) {
            // Respond with SYN-ACK
        	set_response_headers(&pkt_out, &pkt_in, 0);
        	send_packet(&pkt_out, sockfd, (struct sockaddr *) &serv_addr);

            // Received ACK
            n = recv_packet(&pkt_in, sockfd, (struct sockaddr *) &serv_addr, &addr_len);
            if (n > 0 && is_ack_for(&pkt_in, &pkt_out)) {
                printf("Received filename: %s\n", pkt_in.msg);
                break;
            }
        }
    }

    // Test to show client receiving data
    set_response_headers(&pkt_out, &pkt_in, 69);
    send_packet(&pkt_out, sockfd, (struct sockaddr *) &serv_addr);

    close(sockfd);
    return 0;
}
