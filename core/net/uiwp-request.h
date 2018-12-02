#ifndef UIWPREQUEST_H_
#define UIWPREQUEST_H_

#include "contiki-conf.h"

#define CHANNEL_LOWEST 11
#define UIWPREQUEST_FRAME_LENGTH 3

enum {

	UIWPREQUEST_CHANNEL,

    UIWPREQUEST_ORDER,

    UIWPREQUEST_BYTES,

    UIWPREQUEST_NUM,
};

struct uiwprequest_list{
    int channel;
    int order;
    int bytes;
};

int uiwprequest_frame_create(int channel, int order, int bytes, uint8_t *buf);

void uiwprequest_frame_parse(uint8_t *buf, struct uiwprequest_list *uiwprequestlist);

#endif