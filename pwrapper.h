#ifndef PWRAPPER_H
#define PWRAPPER_H

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "packet.h"

#define TIMESTAMP unsigned long long

struct pwrapper {
  char completed;
  TIMESTAMP ts;
  struct packet* packet;
};

TIMESTAMP getTime(); // GETS CURRENT TIME
void printWrapper(struct pwrapper *);
struct pwrapper* createPwrapper(struct packet*);

#endif
