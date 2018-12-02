#include "channel-scan.h"
#include <stdio.h>

#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#ifndef CHANNEL_OFFSET
#define CHANNEL_OFFSET 20
#endif

#ifndef RSSI_MIN
#define RSSI_MIN -100
#endif

#define RSSI_INVALID -128

#include "net/netstack.h"
#include "channel-scan.h"

static int
optimal_channel(int channel_rssi[], int num_of_channel, int channel_min)
{
	int i, min = 0;
	for (i = 0; i < num_of_channel; i++)
	{
		int ch_rssi = channel_rssi[i];
		if (min > ch_rssi
			&& i+channel_min != CHANNEL_OFFSET
			// note: -128 is cc2650 specified value, reserved for modify
			&& ch_rssi != RSSI_INVALID)
			min = ch_rssi;
	}

	for (i = 0; i < num_of_channel; i++)
	{
		if (min == channel_rssi[i] && i+channel_min != CHANNEL_OFFSET)
			return i + channel_min;
	}

	return CHANNEL_SCAN_ERROR;
}

int
channel_scan(void)
{
	int ch;

	// get max and min channel no.
	int num_of_channel, channel_min, channel_max;
	if (NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MIN, &channel_min) != RADIO_RESULT_OK)
		return CHANNEL_SCAN_ERROR;
	if (NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MAX, &channel_max) != RADIO_RESULT_OK)
		return CHANNEL_SCAN_ERROR;

	num_of_channel = channel_max - channel_min + 1;
	PRINTF("number of channels: %d\n", num_of_channel);

	// get rssi of each channel
	int channel_rssi[num_of_channel];
	for (ch = 0; ch< num_of_channel; ch++)
	{
		if (channel_hop(ch + channel_min) == CHANNEL_HOP_ERROR)
			return CHANNEL_SCAN_ERROR;

		if (NETSTACK_RADIO.get_value(RADIO_PARAM_RSSI, &(channel_rssi[ch]))) {
			PRINTF("ch %d RSSI data invalid\n", ch + channel_min);
		}
		
		PRINTF("rssi of channel %d: %d\n", ch + channel_min, channel_rssi[ch]);
	}

	// return to offset channel
	if (channel_hop(CHANNEL_OFFSET) == CHANNEL_HOP_ERROR)
		return CHANNEL_SCAN_ERROR;

	// return the optimal channel
	return optimal_channel(channel_rssi, num_of_channel, channel_min);	
}

int
channel_switch(int channel)
{
	if (NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, channel) != RADIO_RESULT_OK)
		return CHANNEL_HOP_ERROR;

	return CHANNEL_HOP_OK;
}

