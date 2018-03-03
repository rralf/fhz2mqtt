/*
 * fhz2mqtt, a FHZ to MQTT bridge
 *
 * Copyright (c) Ralf Ramsauer, 2018
 *
 * Authors:
 *  Ralf Ramsauer <ralf@ramses-pyramidenbau.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

#include "fhz.h"

#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])

#if 0
const static struct payload payload_hello = {
	.tt = 0xc9,
	.len = 4,
	.data = {0x02, 0x01, 0x1f, 0x60},
};

const static struct payload payload_status_serial = {
	.tt = 0x04,
	.len = 6,
	.data = {0xc9, 0x01, 0x84, 0x57, 0x02, 0x08},
};
#endif

struct fht_handler {
	unsigned char magic[4];
	enum fht_type type;
	int (*handler)(struct fht_decoded *decoded,
		       const unsigned char *payload, ssize_t length);
};

const static int fht_handle_status(struct fht_decoded *decoded,
				   const unsigned char *payload, ssize_t length)
{
	if (length != 4)
		return -EINVAL;

	decoded->status.func = payload[0];
	decoded->status.status = payload[2];
	decoded->status.param = payload[3];
	return 0;
}

const static int fht_handle_ack(struct fht_decoded *decoded,
				const unsigned char *payload, ssize_t length)
{
	if (length != 3)
		return -EINVAL;

	if (payload[0] != 0x3e)
		return -EINVAL;

	decoded->ack.location = payload[2];
	decoded->ack.byte = payload[1];

	return 0;
}

const static struct fht_handler fht_handlers[] = {
	/* status packet */
	{
		.magic = {0x09, 0x09, 0xa0, 0x01},
		.handler = fht_handle_status,
	},
	/* ack packet */
	{
		.magic = {0x83, 0x09, 0x83, 0x01},
		.handler = fht_handle_ack,
	},
};

#define for_each_handler(handlers, handler, counter) \
	for((counter) = 0, handler = (handlers); \
	    (counter) < ARRAY_SIZE((handlers)); \
	    (handler)++, (counter)++)

int fht_decode(const struct payload *payload, struct fht_decoded *decoded)
{
	int i;
	const struct fht_handler *h;

	if (payload->len != 9 || payload->len != 10)
		return -EINVAL;

	for_each_handler(fht_handlers, h, i)
		if (!memcmp(payload->data, h->magic, 4)) {
			decoded->hauscode = *(const struct hauscode*)
				(payload->data + 4);
			decoded->type = h->type;
			return h->handler(decoded, payload->data + 6,
					  payload->len - 6);
		}

	return -EINVAL;
}

int fht80b_set_temp(int fd, struct hauscode *hauscode, float temp)
{
	int x = (int)(temp / 0.5);
	const struct payload payload = {
		.tt = 0x04,
		.len = 7,
		.data = {0x02, 0x01, 0x83, hauscode->upper, hauscode->lower, 0x41, x},
	};

	return fhz_send(fd, &payload);
}
