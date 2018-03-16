#include <string.h>
#include "packet.h"

// For debug purposes, can be removed later
void print_packet_info(struct packet *p)
{
	printf("--\nFlags:");
	if (HAS_FLAG(p, SYN))
		printf(" SYN");
	if (HAS_FLAG(p, ACK))
		printf(" ACK");
	if (HAS_FLAG(p, FIN))
		printf(" FIN");
	printf("\n");

	printf("Seq: %d | Ack: %d | Message length: %d\n--\n", p->seq_num, p->ack_num, p->msg_len);
}

// Set the headers of resp to form a response to prev with a msg of len bytes
void set_response_headers(struct packet *resp, struct packet *prev, int len)
{
	memset(resp, 0, PACKET_HEADER_SIZE); // Reset all headers

	SET_FLAG(resp, ACK);
	resp->msg_len = len;
	resp->ack_num = prev->seq_num;

	if (HAS_FLAG(prev, SYN) && !HAS_FLAG(prev, ACK))
		SET_FLAG(resp, SYN);
}

// Returns 1 if resp acknowledges prev, otherwise 0
int is_ack_for(struct packet *resp, struct packet *prev)
{
	return HAS_FLAG(resp, ACK) && resp->ack_num == prev->seq_num;
}
