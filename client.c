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

    printf("Sending packet %d", p->ack_num);
    if (HAS_FLAG(p, SYN))
        printf(" SYN");
    if (HAS_FLAG(p, FIN))
        printf(" FIN");
    printf("\n");
}

// Receive packet from socket and print formatted output
void recv_packet(struct packet *p, int fd, struct sockaddr *addr, socklen_t *len)
{
    int n = recvfrom(fd, p, PACKET_SIZE, 0, addr, len);
    if (n < 0)
        error("recvfrom");

    printf("Receiving packet %d\n", p->seq_num);
}

int main(int argc, char *argv[])
{
    int sockfd, portno;
    socklen_t addr_len;
    struct sockaddr_in serv_addr;

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

    struct packet pkt_out, pkt_in;

    // Send SYN to start handshake
    pkt_out.seq_num = 0;
    pkt_out.ack_num = 0;
    SET_FLAG(&pkt_out, SYN);
    printf("Sending initial packet:\n");
    print_packet_info(&pkt_out);
    send_packet(&pkt_out, sockfd, (struct sockaddr *) &serv_addr);

    while (1) {
        // Simple test loop sends ACK for each message received
        recv_packet(&pkt_in, sockfd, (struct sockaddr *) &serv_addr, &addr_len);
        set_response_headers(&pkt_out, &pkt_in, 0);
        send_packet(&pkt_out, sockfd, (struct sockaddr *) &serv_addr);

        sleep(1);
    }

    close(sockfd);
    return 0;
}
