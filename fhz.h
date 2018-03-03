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

#include "fht.h"

#define FHZ_MAGIC 0x81

#define BAUDRATE B9600

#define error(...) \
	do { \
		char error_buffer[128]; \
		snprintf(error_buffer, sizeof(error_buffer), __VA_ARGS__);  \
		fprintf(stderr, error_buffer); \
	} while(0)

struct payload {
	unsigned char tt;
	unsigned char len;
	unsigned char data[256];
};

struct fhz_decoded {
	enum {
		FHT,
	} machine;
	union {
		struct fht_decoded fht;
	};
};

int fhz_open_serial(const char *device);
int fhz_send(int fd, const struct payload *payload);
int fhz_handle(int fd, struct fhz_decoded *decoded);
