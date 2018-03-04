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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

#include "fhz.h"

#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])

#define FHT_MODE 0x3e
#define  FHT_MODE_AUTO 0
#define  FHT_MODE_MANU 1
#define  FHT_MODE_HOLI 2
#define FHT_DESIRED_TEMP 0x41
#define FHT_MANU_TEMP 0x45

static int payload_to_fht_temp(const char *payload)
{
	float temp;

	if (sscanf(payload, "%f", &temp) != 1)
		return -EINVAL;

	/* TBd: Range checks! */

	return (unsigned char)(temp/0.5);
}

static int payload_to_mode(const char *payload)
{
	if (!strcasecmp(payload, "auto"))
		return FHT_MODE_AUTO;
	if (!strcasecmp(payload, "manual"))
		return FHT_MODE_MANU;
	if (!strcasecmp(payload, "holiday"))
		return FHT_MODE_HOLI;

	return -EINVAL;
}

struct fht_command {
	unsigned char function_id;
	const char *name;
	int (*input_conversion)(const char *payload);
};

#define for_each_fht_command(commands, command, counter) \
	for((counter) = 0, (command) = (commands); \
	    (counter) < ARRAY_SIZE((commands)); \
	    (counter)++, (command)++)

const static struct fht_command fht_commands[] = {
	/* mode */ {
		.function_id = FHT_MODE,
		.name = "mode",
		.input_conversion = payload_to_mode,
	},
	/* desired temp */ {
		.function_id = FHT_DESIRED_TEMP,
		.name = "desired-temp",
		.input_conversion = payload_to_fht_temp,
	},
	/* manu temp */ {
		.function_id = FHT_MANU_TEMP,
		.name = "manu-temp",
		.input_conversion = payload_to_fht_temp,
	},
};

struct fht_handler {
	unsigned char magic[4];
	enum fht_type type;
	int (*handler)(struct fht_decoded *decoded,
		       const unsigned char *payload, ssize_t length);
};

#define for_each_handler(handlers, handler, counter) \
	for((counter) = 0, handler = (handlers); \
	    (counter) < ARRAY_SIZE((handlers)); \
	    (handler)++, (counter)++)

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

	decoded->ack.unknown = payload[0];
	decoded->ack.byte = payload[1];
	decoded->ack.location = payload[2];

	return 0;
}

const static struct fht_handler fht_handlers[] = {
	/* status packet */
	{
		.type = STATUS,
		.magic = {0x09, 0x09, 0xa0, 0x01},
		.handler = fht_handle_status,
	},
	/* ack packet */
	{
		.type = ACK,
		.magic = {0x83, 0x09, 0x83, 0x01},
		.handler = fht_handle_ack,
	},
};

int fht_decode(const struct payload *payload, struct fht_decoded *decoded)
{
	int i;
	const struct fht_handler *h;

	if (!(payload->len == 9 || payload->len == 10))
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

static int fht_send(int fd, const struct hauscode *hauscode,
		    unsigned char memory, unsigned char value)
{
	const struct payload payload = {
		.tt = 0x04,
		.len = 7,
		.data = {0x02, 0x01, 0x83, hauscode->upper, hauscode->lower,
			 memory, value},
	};

	return fhz_send(fd, &payload);
}

int fht_set(int fd, const struct hauscode *hauscode,
	    const char *command, const char *payload)
{
	const struct fht_command *fht_command;
	unsigned char fht_val;
	bool found = false;
	int i, err;

	for_each_fht_command(fht_commands, fht_command, i)
		if (!strcmp(fht_command->name, command)) {
			found = true;
			break;
		}

	if (!found)
		return -EINVAL;

	err = fht_command->input_conversion(payload);
	if (err < 0)
		return err;

	fht_val = err;

	return fht_send(fd, hauscode, fht_command->function_id, fht_val);
}
