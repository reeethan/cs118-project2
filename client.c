#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
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
int recv_packet(struct packet *p, int fd, struct sockaddr *addr, socklen_t *len)
{
    int n = recvfrom(fd, p, PACKET_SIZE, 0, addr, len);
    if (n < 0 && errno != EAGAIN)
        error("recvfrom");
    if (n > 0)
        printf("Receiving packet %d\n", p->seq_num);

    return n;
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    socklen_t addr_len;
    struct sockaddr_in serv_addr;
    struct packet pkt_out, pkt_in;

    if (argc < 3) {
        fprintf(stderr,"Usage: ./client HOST PORT FILENAME\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // create socket
    if (sockfd < 0)
        error("ERROR opening socket");

    // Set receive timeout of 500ms
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        error("setsockopt");
    }

    memset((char *) &serv_addr, 0, sizeof(serv_addr)); // reset memory

    // fill in address info
    portno = atoi(argv[2]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    // Loop until handshake is completed
    while (1) {
        // Send SYN to start handshake
        memset(&pkt_out, 0, PACKET_HEADER_SIZE);
        SET_FLAG(&pkt_out, SYN);
        send_packet(&pkt_out, sockfd, (struct sockaddr *) &serv_addr);
        n = recv_packet(&pkt_in, sockfd, (struct sockaddr *) &serv_addr, &addr_len);

        // If SYN-ACK is received, we can start sending messages
        if (n > 0 && HAS_FLAG(&pkt_in, SYN) && HAS_FLAG(&pkt_in, ACK))
            break;
    }

    // Send requested filename
    strcpy(pkt_out.msg, argv[3]);
    set_response_headers(&pkt_out, &pkt_in, strlen(argv[3]));
    while (1) {
        send_packet(&pkt_out, sockfd, (struct sockaddr *) &serv_addr);
        n = recv_packet(&pkt_in, sockfd, (struct sockaddr *) &serv_addr, &addr_len);

        // Message was ACKed, start receiving file data
        if (n > 0 && is_ack_for(&pkt_out, &pkt_in))
            break;
    }

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
