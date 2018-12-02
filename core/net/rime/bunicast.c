#include "net/rime/rime.h"
#include "net/rime/bunicast.h"
#include <string.h>

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#ifndef BLOCK_SIZE_OFFSET
#define BLOCK_SIZE_OFFSET 8
#endif

#ifndef FRAME_DATA_SIZE
#define FRAME_DATA_SIZE 96
#endif

static void 
recv_from_unciast(struct unicast_conn *unicast, const linkaddr_t *from)
{
	struct bunicast_conn *c = (struct bunicast_conn *)unicast;

	if (c->u->recv) {
		c->u->recv(c, from);
	}
}

static void
sent_by_unicast(struct unicast_conn *unicast, int status, int num_tx)
{
	struct bunicast_conn *c = (struct bunicast_conn *)unicast;

	static int s = 0;

	if (c->counter == 0)
		s = 0;

	s += status;

	PRINTF("status: %d, counter: %d, total_num_tx: %d\n", 
		status, c->counter, c->total_num_tx);

	if (++c->counter == c->total_num_tx) {
		if (c->u->sent) {
			c->u->sent(c, s);
		}

// #if DEBUG
// 		int ds;
// 		for (ds = 0; ds < c->total_num_tx; ds++)
// 			PRINTF("No. %d packet status: %d\n", ds + 1, s[ds]);
// #endif

		// c->counter = 0;
		// c->total_num_tx = 0;
	}
}

static const struct unicast_callbacks buc = {recv_from_unciast, sent_by_unicast};

void
bunicast_open(struct bunicast_conn *c, uint16_t channel, const struct bunicast_callbacks *u)
{
	unicast_open(&c->c, channel, &buc);
	c->u = u;
	c->block_size = BLOCK_SIZE_OFFSET;
	c->total_num_tx = 0;
	c->counter = 0;
}

void
bunicast_close(struct bunicast_conn *c)
{
	unicast_close(&c->c);
}

void
bunicast_send(struct bunicast_conn *c, const linkaddr_t *receiver, char *data)
{
	int num_of_tx;
	// max data size = FRAME_DATA_SIZE * c->block_size
	int data_len = MIN(strlen(data), FRAME_DATA_SIZE * c->block_size);

	// total number of unicast
	c->total_num_tx = data_len / FRAME_DATA_SIZE;
	if (data_len % FRAME_DATA_SIZE != 0) 
		c->total_num_tx += 1;
		
	PRINTF("Total number of transmission: %d, sent to %d.%d\n",
		c->total_num_tx, receiver->u8[0], receiver->u8[1]);

	char *data_ptr;

	// block transmission
	for (num_of_tx = 0; num_of_tx < c->total_num_tx; num_of_tx++) {
		data_ptr = data + FRAME_DATA_SIZE * num_of_tx;
		packetbuf_copyfrom(data_ptr, MIN(strlen(data_ptr), FRAME_DATA_SIZE));
		unicast_send(&c->c, receiver);
	}

	c->counter = 0;
}

int
bunicast_size(struct bunicast_conn *c, int size)
{
	if(size > QUEUEBUF_CONF_NUM)
		return 1;

	c->block_size = size;

	return 0;
}
