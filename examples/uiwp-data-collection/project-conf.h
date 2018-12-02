// To compile UIWP for sink node please uncomment nullrdc-driver
// To compile UIWP for sensor node please uncomment contikimac-driver


#define CHANNEL_OFFSET 26

#define FRAME_DATA_SIZE 100

#define BLOCK_SIZE_OFFSET 8

#define NETSTACK_CONF_RDC     nullrdc_driver

// #define NETSTACK_CONF_RDC     contikimac_driver

#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 16

#define CSMA_CONF_MAX_FRAME_RETRIES 0
