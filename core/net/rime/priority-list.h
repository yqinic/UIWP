#ifndef PRIORITYLIST_H_
#define PRIORITYLIST_H_

#include "contiki-conf.h"
#include "net/rime/rime.h"

struct uiwppriority_list {
    struct uiwppriority_list *next;
    int rssi;
    uint8_t re_tx;
    linkaddr_t addr;
    struct uiwpack_list uiwpacklist;
};

int uiwpplist_create(int rssi, const linkaddr_t *addr, struct uiwpack_list *uiwpacklist);

void * uiwpplist_get();

int uiwpplist_retx_update(struct uiwppriority_list *uiwpplist);

int uiwpplist_free();

#endif