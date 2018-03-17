#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include "packet.h"
#include "pwrapper.h"
#define WIN_SZ 5
#define TIMEOUT 500

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
    if (n < 0 && errno != EAGAIN)
        error("recvfrom");
    if (n > 0) {
    	printf("Receiving packet %d", p->ack_num);
    	if (HAS_FLAG(p, SYN))
        	printf(" SYN");
    	if (HAS_FLAG(p, ACK))
			printf(" ACK");
    	if (HAS_FLAG(p, FIN))
        	printf(" FIN");
    	printf("\n");
    }

    return n;
}


/* Queue state varaibles */
int front = 0;
int rear = -1;
int itemCount = 0;
struct pwrapper* win[WIN_SZ];

/* Queue Operations */
int winEmpty() {
  return itemCount == 0;
}

int winFull() {
  return itemCount == WIN_SZ;
}

int winSz() {
  return itemCount;
}

struct pwrapper* winPeek() {
   return winEmpty() ? NULL : win[front];
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

struct pwrapper* winPop() {
  if(!winPeek()) return NULL;
  struct pwrapper* data = win[front++];

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
    int i = (front == 0) ? (WIN_SZ - 1) : front - 1;

    do {
      i = (i == WIN_SZ - 1) ? 0 : i + 1;
      printWrapper(win[i]);
    } while(i != rear);

    printf("----------------------------------\n\n");
  }
  else
    printf("WINDOW IS EMPTY\n");
}

void markPackets(int ack) {
  if(!winEmpty()) {
    int i = (front == 0) ? (WIN_SZ - 1) : front - 1;

    do {
      i = (i == WIN_SZ - 1) ? 0 : i + 1;
      if(win[i]->packet->seq_num == ack) win[i]->completed = 1;
    } while(i != rear);
  }
}

int sweepPackets() {
  if(!winEmpty() && win[front]->completed) {
    struct pwrapper* temp = winPop();
    return 1;
  }
  else {
    return 0;
  }
}

struct packet* getTimedOutPacket() {
  if(!winEmpty()) {
    TIMESTAMP currTime = getTime();
    int i = (front == 0) ? (WIN_SZ - 1) : front - 1;

    do {
      i = (i == WIN_SZ - 1) ? 0 : i + 1;
      // if elapsed time is greater than 500 and its not completed, return that
      //printf("ELAPSED TIME FOR PACKET %d: %llu\n", win[i]->packet->seq_num, currTime - win[i]->ts); DEBUG
      if(currTime - win[i]->ts >= TIMEOUT && !win[i]->completed) {
        //printf("PACKET TIMED OUT\n"); DEBUG
        win[i]->ts = currTime;
        return win[i]->packet;
      }
    } while(i != rear);
  }
  // if no such packet has timed out, return NULL
  return NULL;
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

    // Set small receive timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        error("setsockopt");
    }

    memset((char *) &serv_addr, 0, sizeof(serv_addr)); // reset memory

    // fill in address info
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    struct packet pkt_out, pkt_in;
    memset(&pkt_out, 0, PACKET_HEADER_SIZE);
    while(1) {
        // Wait to receive packet with SYN flag
        n = recv_packet(&pkt_in, sockfd, (struct sockaddr *) &serv_addr, &addr_len);
        if(n > 0) {
            if (HAS_FLAG(&pkt_in, SYN)) {
                // Respond with SYN-ACK
                set_response_headers(&pkt_out, &pkt_in, 0);
                send_packet(&pkt_out, sockfd, (struct sockaddr *) &serv_addr);

                // Received ACK
                n = recv_packet(&pkt_in, sockfd, (struct sockaddr *) &serv_addr, &addr_len);
            } else if (is_ack_for(&pkt_in, &pkt_out)) {
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
    int current_seq = pkt_in.ack_num + 1;
    while(!eof) {
      if(!winFull() && !eof) {
        struct packet* new_pkt_out = (struct packet *) malloc(sizeof(struct packet));
        memset(new_pkt_out->msg, 0, MSG_SIZE);
        int n = read(fd, new_pkt_out->msg, MSG_SIZE);
        if(n > 0) {
          set_response_headers(new_pkt_out, &pkt_in, n);
          new_pkt_out->seq_num = current_seq;
          send_packet(new_pkt_out, sockfd, (struct sockaddr *) &serv_addr);
          current_seq = (current_seq + n) % SEQ_NUM_MAX;

          /* save packet wrapper to the window */
          struct pwrapper* pTemp = createPwrapper(new_pkt_out);
          winPush(pTemp);
          //winDump(); DEBUG
        }
        else eof = 1;
      }
      // have a function that returns a pointer to the first packet whose elapsed time is greater than 500 ms
      // or returns null if that doesnt exist
      struct packet* packetToRetransmit = getTimedOutPacket();
      if(packetToRetransmit) {
        //printf("***** GOT A TIMEOUT *****\n"); DEBUG
        print_packet_info(packetToRetransmit);
        send_packet(packetToRetransmit, sockfd, (struct sockaddr *) &serv_addr);
        //printf("*************************\n\n"); DEBUG
      }

      recv_packet(&pkt_in, sockfd, (struct sockaddr *) &serv_addr, &addr_len);

      // mark the appropriate packet as completed
      markPackets(pkt_in.ack_num);
      /* DEBUG
      printf("AFTER MARKING\n");
      winDump();
      */

      // Remove completed packets from the front until you run into an incompleted packet
      while(sweepPackets());
      /* DEBUG
      printf("AFTER SWEEPING\n");
      winDump();
      */
    }
    /* SEND FIN */
    set_response_headers(&pkt_out, &pkt_in, 0);
    pkt_out.seq_num = current_seq + 1;
    SET_FLAG(&pkt_out, FIN);
    send_packet(&pkt_out, sockfd, (struct sockaddr *) &serv_addr);

    close(fd);
    close(sockfd);
    return 0;
}
