#include "pwrapper.h"
#include <sys/time.h>

TIMESTAMP getTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return ((unsigned long long)(tv.tv_sec) * 1000) +
    ((unsigned long long)(tv.tv_usec) / 1000);
}

void printWrapper(struct pwrapper* p) {
  printf("=== PACKET WRAPPER ===\n");
  print_packet_info(p->packet);
  printf("STATUS: ");
  if(p->completed) printf("PACKET CONFIRMED!\n");
  else printf("PACKET NOT CONFIRMED\n");
  printf("TIMESTAMP: %llu\n", p->ts);
  printf("======================\n");
}

struct pwrapper* createPwrapper(struct packet* pkt){
  struct pwrapper* p = (struct pwrapper *) malloc(sizeof(struct pwrapper));
  p->completed = 0;
  p->ts = getTime();
  p->packet = pkt;

  return p;
}
