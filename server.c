#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include "packet.h"
#include "pwrapper.h"
#define WIN_SZ 5

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
    if (n > 0) {
    	printf("Receiving packet %d\n", p->ack_num);
      //print_packet_info(p);
    }

    return n;
}

/* Queue state varaibles */
int front = 0;
int rear = -1;
int itemCount = 0;
struct pwrapper* win[WIN_SZ];

/* Queue Operations */
struct pwrapper winPeek() {
   return *win[front];
}

int winEmpty() {
  return itemCount == 0;
}

int winFull() {
  return itemCount == WIN_SZ;
}

int winSz() {
  return itemCount;
}

void winPush(struct pwrapper* data) {
  if(!winFull()) {
    if(rear == WIN_SZ-1) {
       rear = -1;
    }

    win[++rear] = data;
    itemCount++;
  }
}

struct pwrapper winPop() {
  struct pwrapper data = *win[front++];

  if(front == WIN_SZ) {
    front = 0;
  }

  itemCount--;
  return data;
}

/* Display the contents of window */
void winDump() {
  if(!winEmpty()) {
    printf("----- CONTENTS OF THE WINDOW -----\n");
    int i = front;
    while(i <= rear) {
      printWrapper(win[i]);
      if(i == (WIN_SZ - 1)) i = 0;
      else i++;
    }
  }
  else
    printf("WINDOW IS EMPTY\n");
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

    int fd = open(pkt_in.msg, O_RDONLY);
    if(fd < 0) { // if file does not exist, send back a 404 error not found
    	strcpy(pkt_out.msg, "404 Not Found\n");
    	set_response_headers(&pkt_out, &pkt_in, strlen(pkt_out.msg));
      send_packet(&pkt_out, sockfd, (struct sockaddr *) &serv_addr);
    }

    // send a message sized packet as long as there is data to send and window isnt full
    int eof = 0; // end of file is false
    while(!eof) {
      if(!winFull() && !eof) {
        if(read(fd, pkt_out.msg, MSG_SIZE) > 0) {
          set_response_headers(&pkt_out, &pkt_in, strlen(pkt_out.msg));
          send_packet(&pkt_out, sockfd, (struct sockaddr *) &serv_addr);

          /* save packet wrapper to the window */
          struct pwrapper* pTemp = createPwrapper(&pkt_out);
          winPush(pTemp);
          winDump();

          recv_packet(&pkt_in, sockfd, (struct sockaddr *) &serv_addr, &addr_len);

          // search window for packet with ACK # received from client
          // iteratively check the front of the window for completed PACKETs
          // pop until you reach an uncomepleted paket. 
        }
        else eof = 1;
      }
    }

    close(sockfd);
    return 0;
}
