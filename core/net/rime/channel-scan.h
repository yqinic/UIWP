#ifndef CHANNEL_SCAN_H
#define CHANNEL_SCAN_H

#include "contiki.h"

enum {
    CHANNEL_SWITCH_OK,
    CHANNEL_SWITCH_ERROR
};

enum {
    CHANNEL_SCAN_OK,
    CHANNEL_SCAN_ERROR
};

int channel_scan(void);

int channel_switch(int channel); 

#endif