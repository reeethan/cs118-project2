#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include "packet.h"

void error(char *msg)
{
    perror(msg);
    exit(1);
}

struct recv_buffer {
    int dest_fd;
    off_t base;
    struct packet packets[RECV_WINDOW];
    struct packet pkt_out;
    struct packet *last_pkt;
};

struct packet *get_free_pkt(struct recv_buffer *rbuf)
{
    struct packet *pkt;
    for (pkt = rbuf->packets; pkt <= rbuf->packets + RECV_WINDOW; pkt++)
        if (pkt->seq_num < rbuf->base % SEQ_NUM_MAX)
            break;

    return pkt;
}

// Send packet to socket and print formatted output
void send_packet(struct packet *p, int fd, struct sockaddr *addr)
{
    int n = sendto(fd, p, PACKET_HEADER_SIZE + p->msg_len, 0, addr, sizeof(struct sockaddr));
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
int recv_packet(struct recv_buffer *rbuf, int fd, struct sockaddr *addr, socklen_t *len)
{
    struct packet *p = get_free_pkt(rbuf);
    int n = recvfrom(fd, p, PACKET_SIZE, 0, addr, len);
    if (n < 0 && errno != EAGAIN)
        error("recvfrom");
    if (n > 0) {
        printf("Receiving packet %d\n", p->seq_num);
        rbuf->last_pkt = (struct packet *) p;
    }

    return n;
}

void send_response(struct recv_buffer *rbuf, int fd, struct sockaddr *addr, char *msg)
{
    int msg_len = msg ? strlen(msg) : 0;
    if (msg)
        strcpy(rbuf->pkt_out.msg, msg);

    set_response_headers(&rbuf->pkt_out, rbuf->last_pkt, msg_len);
    send_packet(&rbuf->pkt_out, fd, addr);
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n, fd;
    struct sockaddr_in serv_addr;
    socklen_t addr_len = sizeof(serv_addr);

    struct recv_buffer rbuf;

    if (argc < 4) {
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

    // Initialize receive buffer
    rbuf.base = 1;
    for (int i = 0; i < 5; i++)
        memset(&rbuf.packets[i], 0, PACKET_HEADER_SIZE);

    // Loop until handshake is completed
    memset(&rbuf.pkt_out, 0, PACKET_HEADER_SIZE);
    SET_FLAG(&rbuf.pkt_out, SYN);
    while (1) {
        // Send SYN to start handshake
        send_packet(&rbuf.pkt_out, sockfd, (struct sockaddr *) &serv_addr);
        n = recv_packet(&rbuf, sockfd, (struct sockaddr *) &serv_addr, &addr_len);

        // If SYN-ACK is received, we can start sending messages
        if (n > 0 && HAS_FLAG(rbuf.last_pkt, SYN) && HAS_FLAG(rbuf.last_pkt, ACK))
			break;
    }

    // Send requested filename
    while (1) {
        send_response(&rbuf, sockfd, (struct sockaddr *) &serv_addr, argv[3]);
        n = recv_packet(&rbuf, sockfd, (struct sockaddr *) &serv_addr, &addr_len);

        // Message was ACKed, start receiving file data
        if (n > 0 && is_ack_for(rbuf.last_pkt, &rbuf.pkt_out))
            break;
    }

    // Open
    rbuf.dest_fd = open("received.data", O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (rbuf.dest_fd < 0)
        error("open");

    while (!HAS_FLAG(rbuf.last_pkt, FIN)) {
        if (n > 0) {
            if (rbuf.last_pkt->seq_num == rbuf.base % SEQ_NUM_MAX) {
                struct packet *start = &rbuf.packets[0];
                struct packet *pkt = rbuf.last_pkt;
                while (pkt <= &rbuf.packets[RECV_WINDOW])
                     if (pkt->seq_num == rbuf.base % SEQ_NUM_MAX) {
                        int n = write(rbuf.dest_fd, pkt->msg, pkt->msg_len);
                        if (n < 0)
                            error("write");

                        printf("Wrote bytes %lld-%lld\n", rbuf.base, rbuf.base + n);
                        rbuf.base += n;

                        if (pkt == start)
                            start++;
                        pkt = start;
                    } else pkt++;
            }

        }

        n = recv_packet(&rbuf, sockfd, (struct sockaddr *) &serv_addr, &addr_len);
        send_response(&rbuf, sockfd, (struct sockaddr *) &serv_addr, NULL);
    }

    printf("End of file, received %lld bytes\n", rbuf.base);

    close(rbuf.dest_fd);
    close(sockfd);
    return 0;
}
