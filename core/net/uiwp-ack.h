#ifndef UIWPACK_H_
#define UIWPACK_H_

#include "contiki-conf.h"

#ifndef BATTERY_VOLUMN_OFFSET
#define BATTERY_VOLUMN_OFFSET 2400
#endif

#ifndef ANTENNA_TYPE_OFFSET
#define ANTENNA_TYPE_OFFSET 0
#endif

#ifndef ANTENNA_ORIENTATION_OFFSET
#define ANTENNA_ORIENTATION_OFFSET 0
#endif

#define UIWPACK_ACK (0x01)
#define UIWPACK_FRAME_LENGTH 11
#define DATA_PRIORITY_BIN 6
#define DATA_BYTES_OCT 2
#define BATTERY_VOLTAGE_OCT 2
#define BATTERY_VOLUMN_OCT 2
#define ANTENNA_TYPE_OCT 1
#define ANTENNA_ORIENTATION_OCT 2

#ifndef UIWP_PRIORITY_LEVEL_MAX
#define UIWP_PRIORITY_LEVEL_MAX 3
#endif

enum {
    // how much data to be transferred
	UIWPACK_DATA_PRIORITY,
    UIWPACK_DATA_BYTES,

    // node battery information
    UIWPACK_BATTERY_VOLTAGE,
    UIWPACK_BATTERY_VOLUMN,

    // node antenna information
    UIWPACK_ANTENNA_TYPE,
    UIWPACK_ANTENNA_ORIENTATION,

    // camera inspection information
    UIWPACK_CAMERA,

    UIWPACK_NUM,
};

struct uiwpack_list{
    int priority;
    int bytes;
    int voltage;
    int volumn;
    int type;
    int orientation;
    int camera;
};

int uiwpack_init();

int uiwpack_set_attr(int type, const int value);

int uiwpack_get_attr(int type);

int uiwpack_frame_create(int num, uint8_t *buf);

void uiwpack_frame_parse(uint8_t *buf, struct uiwpack_list *uiwpacklist);

#endif
