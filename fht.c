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

#define FHT_IS_VALVE 0x00
#define FHT_MODE 0x3e
#define  FHT_MODE_AUTO 0
#define  FHT_MODE_MANU 1
#define  FHT_MODE_HOLI 2
#define FHT_DESIRED_TEMP 0x41
#define FHT_IS_TEMP_LOW 0x42
#define FHT_IS_TEMP_HIGH 0x43
#define FHT_STATUS 0x44 /* exceptional case! */
#define FHT_MANU_TEMP 0x45

const static char s_mode_auto[] = "auto";
const static char s_mode_holiday[] = "holiday";
const static char s_mode_manual[] = "manual";

static int payload_to_fht_temp(const char *payload)
{
	float temp;

	if (sscanf(payload, "%f", &temp) != 1)
		return -EINVAL;

	/* TBd: Range checks! */

	return (unsigned char)(temp/0.5);
}

static void fht_temp_to_str(char *dst, int len, unsigned char temp)
{
	snprintf(dst, len, "%0.1f", (float)temp * 0.5);
}

static int payload_to_mode(const char *payload)
{
	if (!strcasecmp(payload, s_mode_auto))
		return FHT_MODE_AUTO;
	if (!strcasecmp(payload, s_mode_manual))
		return FHT_MODE_MANU;
	if (!strcasecmp(payload, s_mode_manual))
		return FHT_MODE_HOLI;

	return -EINVAL;
}

static void mode_to_str(char *dst, int len, unsigned char mode)
{
	switch (mode) {
	case FHT_MODE_AUTO:
		strncpy(dst, s_mode_auto, len);
		break;
	case FHT_MODE_MANU:
		strncpy(dst, s_mode_manual, len);
		break;
	case FHT_MODE_HOLI:
		strncpy(dst, s_mode_holiday, len);
		break;
	default:
		strncpy(dst, "unknown", len);
		break;
	}
}

static int input_not_accepted(const char *payload)
{
	return -EPERM;
}

static void fht_is_temp_to_str(char *dst, int len, unsigned char value)
{
	snprintf(dst, len, "%u", value);
}

static void fht_percentage_to_str(char *dst, int len, unsigned char value)
{
	snprintf(dst, len, "%u%%", value);
}

struct fht_command {
	unsigned char function_id;
	const char *name;
	int (*input_conversion)(const char *payload);
	void (*output_conversion)(char *dst, int len, unsigned char value);
};

#define for_each_fht_command(commands, command, counter) \
	for((counter) = 0, (command) = (commands); \
	    (counter) < ARRAY_SIZE((commands)); \
	    (counter)++, (command)++)

const static struct fht_command fht_commands[] = {
	/* is valve */ {
		.function_id = FHT_IS_VALVE,
		.name = "is-valve",
		.input_conversion = input_not_accepted,
		.output_conversion = fht_percentage_to_str,
	},
	/* mode */ {
		.function_id = FHT_MODE,
		.name = "mode",
		.input_conversion = payload_to_mode,
		.output_conversion = mode_to_str,
	},
	/* desired temp */ {
		.function_id = FHT_DESIRED_TEMP,
		.name = "desired-temp",
		.input_conversion = payload_to_fht_temp,
		.output_conversion = fht_temp_to_str,
	},
	/* is temp low */ {
		.function_id = FHT_IS_TEMP_LOW,
		.name = "is-temp-low",
		.input_conversion = input_not_accepted,
		.output_conversion = fht_is_temp_to_str,
	},
	/* is temp high */ {
		.function_id = FHT_IS_TEMP_HIGH,
		.name = "is-temp-high",
		.input_conversion = input_not_accepted,
		.output_conversion = fht_is_temp_to_str,
	},
	/* manu temp */ {
		.function_id = FHT_MANU_TEMP,
		.name = "manu-temp",
		.input_conversion = payload_to_fht_temp,
		.output_conversion = fht_temp_to_str,
	},
};

int fht_decode(const struct payload *payload, struct fht_decoded *decoded)
{
	const static unsigned char magic_ack[] = {0x83, 0x09, 0x83, 0x01};
	const static unsigned char magic_status[] = {0x09, 0x09, 0xa0, 0x01};
	const struct fht_command *fht_command;
	unsigned char cmd, val;
	int i;

	if (payload->len < 9)
		return -EINVAL;

	if (!memcmp(payload->data, magic_ack, sizeof(magic_ack))) {
		decoded->type = ACK;
		cmd = payload->data[6];
		val = payload->data[7];
	} else if (!memcmp(payload->data, magic_status, sizeof(magic_status))) {
		if (payload->len != 10)
			return -EINVAL;
		decoded->type = STATUS;
		cmd = payload->data[6];
		val = payload->data[9];
	} else
		return -EINVAL;

	decoded->hauscode = *(const struct hauscode*)(payload->data + 4);
	decoded->topic2 = NULL;

	if (cmd == FHT_STATUS) {
		decoded->topic1 = "window";
		snprintf(decoded->value1, sizeof(decoded->value1), "%s",
			 val & (1 << 5) ? "open" : "close");
		decoded->topic2 = "battery";
		snprintf(decoded->value2, sizeof(decoded->value2), "%s",
			 val & (1 << 0) ? "empty" : "ok");
		return 0;
	}

	for_each_fht_command(fht_commands, fht_command, i) {
		if (fht_command->function_id != cmd)
			continue;
		decoded->topic1 = fht_command->name;
		fht_command->output_conversion(decoded->value1,
					       sizeof(decoded->value1), val);
		return 0;
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
