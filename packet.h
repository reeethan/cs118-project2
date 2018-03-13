#ifndef PACKET_H
#define PACKET_H

#include <sys/types.h>
#include <stdio.h>

// Constants
#define SEQ_NUM_MAX 30720
#define PACKET_SIZE 1024
#define PACKET_HEADER_SIZE (4 * sizeof(int))
#define MSG_SIZE (PACKET_SIZE - PACKET_HEADER_SIZE)

#define FLAG_SYN 0x1
#define FLAG_ACK 0x2
#define FLAG_FIN 0x4

// Handy dandy macros
#define HAS_FLAG(p, F) ((p)->flags & FLAG_##F)
#define SET_FLAG(p, F) ((p)->flags |= FLAG_##F)

struct packet {
	int flags;
	int seq_num;
	int ack_num;
	int msg_len;
	char msg[MSG_SIZE];
};

int is_ack_for(struct packet* resp, struct packet* prev);


void print_packet_info(struct packet *p);
void set_response_headers(struct packet* prev, struct packet* resp, int len);

#endif /* PACKET_H */
