#include "net/uiwp-ack.h"
#include "lib/sensors.h"
#include "dev/batmon-sensor.h"
#include "contiki.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c\n"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 
#else
#define PRINTF(...)
#endif

static int uiwp_acks[UIWPACK_NUM];

static int 
uiwpack_update(int num)
{
    uiwp_acks[UIWPACK_DATA_PRIORITY] = 2;
    uiwp_acks[UIWPACK_DATA_BYTES] = num;

    SENSORS_ACTIVATE(batmon_sensor);
    uiwp_acks[UIWPACK_BATTERY_VOLTAGE] = batmon_sensor.value(BATMON_SENSOR_TYPE_VOLT);
    SENSORS_DEACTIVATE(batmon_sensor);

    return 1;
}

int
uiwpack_init()
{
    uiwp_acks[UIWPACK_DATA_PRIORITY] = 0;
    uiwp_acks[UIWPACK_DATA_BYTES] = 0;
    uiwp_acks[UIWPACK_BATTERY_VOLTAGE] = 0;
    uiwp_acks[UIWPACK_CAMERA] = 0;

    uiwp_acks[UIWPACK_BATTERY_VOLUMN] = BATTERY_VOLUMN_OFFSET;
    uiwp_acks[UIWPACK_ANTENNA_TYPE] =  ANTENNA_TYPE_OFFSET;
    uiwp_acks[UIWPACK_ANTENNA_ORIENTATION] = ANTENNA_ORIENTATION_OFFSET;

    return 1;
}

int 
uiwpack_set_attr(int type, const int value) 
{
    uiwp_acks[type] = value;
    return 1;
}

int
uiwpack_get_attr(int type)
{
    return uiwp_acks[type];
}

int 
uiwpack_frame_create(int num, uint8_t *buf)
{
    uiwpack_update(num);

    uint8_t pos = 0;

    buf[pos++] = ((1 & 3) << 6) | (uiwp_acks[UIWPACK_DATA_PRIORITY] & 63);
    
    buf[pos++] = (uiwp_acks[UIWPACK_DATA_BYTES] >> 8) & 0xff;
    buf[pos++] = uiwp_acks[UIWPACK_DATA_BYTES] & 0xff;

    buf[pos++] = (uiwp_acks[UIWPACK_BATTERY_VOLTAGE] >> 8) & 0xff;
    buf[pos++] = uiwp_acks[UIWPACK_BATTERY_VOLTAGE] & 0xff;

    buf[pos++] = (uiwp_acks[UIWPACK_BATTERY_VOLUMN] >> 8) & 0xff;
    buf[pos++] = uiwp_acks[UIWPACK_BATTERY_VOLUMN] & 0xff;

    buf[pos++] = uiwp_acks[UIWPACK_ANTENNA_TYPE] & 0xff;

    buf[pos++] = (uiwp_acks[UIWPACK_ANTENNA_ORIENTATION] >> 8) & 0xff;
    buf[pos++] = uiwp_acks[UIWPACK_ANTENNA_ORIENTATION] & 0xff;

    buf[pos++] = uiwp_acks[UIWPACK_CAMERA] & 0xff;

#if DEBUG
    int i;
	for (i = 0; i < UIWPACK_FRAME_LENGTH; i++) {
		PRINTF(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(buf[i]));
	}
#endif

    return (int)pos;
}

void 
uiwpack_frame_parse(uint8_t *buf, struct uiwpack_list *uiwpacklist)
{

    uiwpacklist->priority = buf[0] & 63;
    uiwpacklist->bytes = (buf[1] << 8) + buf[2];
    uiwpacklist->voltage = (buf[3] << 8) + buf[4];
    uiwpacklist->volumn = (buf[5] << 8) + buf[6];
    uiwpacklist->type = buf[7];
    uiwpacklist->orientation = (buf[8] << 8) + buf[9];
    uiwpacklist->camera = buf[10];

    PRINTF("%d\n%d\n%d\n%d\n%d\n%d\n%d\n", uiwpacklist->priority,
        uiwpacklist->bytes, uiwpacklist->voltage, uiwpacklist->volumn,
        uiwpacklist->type, uiwpacklist->orientation, uiwpacklist->camera);

    return;
}