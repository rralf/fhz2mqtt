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
#include <string.h>

struct payload;

struct hauscode {
	unsigned char upper;
	unsigned char lower;
} __attribute__((packed));

enum fht_type {STATUS, ACK};

struct fht_decoded {
	enum fht_type type;
	struct hauscode hauscode;
	union {
		struct {
			unsigned char unknown;
			unsigned char location;
			unsigned char byte;
		} ack;

		struct {
			unsigned char func;
			unsigned char status;
			unsigned char param;
		} status;
	};
};

static inline int hauscode_from_string(const char *string,
				       struct hauscode *hauscode)
{
	int i;

	if (strlen(string) != 4)
		return -EINVAL;

	for (i = 0; i < 4; i++)
		if (!isdigit(string[i]))
			return -EINVAL;

	hauscode->upper = (string[0] - '0') * 10;
	hauscode->upper += (string[1] - '0') * 1;
	hauscode->lower = (string[2] - '0') * 10;
	hauscode->lower += (string[3] - '0') * 1;

	return 0;
}

int fht_decode(const struct payload *payload, struct fht_decoded *decoded);
int fht_send(int fd, struct hauscode *hauscode,
	     unsigned char fht_cmd, unsigned char fht_val);
