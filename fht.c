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

#define FHT_TEMP_OFF 5.5
#define FHT_TEMP_ON 30.5

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
#define FHT_DAY_TEMP 0x82
#define FHT_NIGHT_TEMP 0x84
#define FHT_WINDOW_OPEN_TEMP 0x8a

struct fht_message_raw {
	unsigned char subfun;
	unsigned char status;
	unsigned char value;
};

struct fht_command {
	unsigned char function_id;
	const char *name;
	int (*input_conversion)(const char *payload);
	int (*output_conversion)(char *dst, int len,
				 const struct fht_message_raw *raw);
};

#define for_each_fht_command(commands, command, counter) \
	for((counter) = 0, (command) = (commands); \
	    (counter) < ARRAY_SIZE((commands)); \
	    (counter)++, (command)++)

const static char s_mode_auto[] = "auto";
const static char s_mode_holiday[] = "holiday";
const static char s_mode_manual[] = "manual";

static unsigned char temp_low;

static int payload_to_fht_temp(const char *payload)
{
	float temp;

	if (!strcasecmp(payload, "off")) {
		temp = FHT_TEMP_OFF;
		goto temp_out;
	} else if (!strcasecmp(payload, "on")) {
		temp = FHT_TEMP_ON;
		goto temp_out;
	} else if (sscanf(payload, "%f", &temp) != 1)
		return -EINVAL;

	if (temp < FHT_TEMP_OFF || temp > FHT_TEMP_ON)
		return -ERANGE;

temp_out:
	return (unsigned char)(temp/0.5);
}

static int fht_temp_to_str(char *dst, int len,
			   const struct fht_message_raw *raw)
{
	snprintf(dst, len, "%0.1f", (float)raw->value * 0.5);
	return 0;
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

static int mode_to_str(char *dst, int len,
		       const struct fht_message_raw *raw)
{
	switch (raw->value) {
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
		return -EINVAL;
		break;
	}

	return 0;
}

static int input_not_accepted(const char *payload)
{
	return -EPERM;
}

static int fht_is_temp_low(char *dst, int len,
			   const struct fht_message_raw *raw)
{
	temp_low = raw->value;
	return -EAGAIN;
}

static int fht_is_temp_high_to_str(char *dst, int len,
				   const struct fht_message_raw *raw)
{
	snprintf(dst, len, "%0.2f",
		 ((float)temp_low + (float)raw->value* 256)/10.0);
	return 0;
}

static int fht_percentage_to_str(char *dst, int len,
				 const struct fht_message_raw *raw)
{
	unsigned char l, r;
	unsigned char valve = raw->value;

	l = (raw->status >> 4) & 0x0f;
	r = raw->status & 0x0f;

	/* actuator changed state. e.g., the valve */
	if (l == 0x2) {
	 /* actuator didn't change */
	} else if (l == 0xa) {
	}

	switch (r) {
	case 0x1: /* 30.5 or ON on fht80b */
		valve = 0xff;
		break;
	case 0x2: /* 5.5 or OFF on fht80b */
		valve = 0;
		break;
	case 0x0: /* value contains valve state */
	case 0x6:
		break;
	case 0x8: /* value contains OFFSET setting */
		return -EINVAL;
		/* TBD: implement offset transmission */
		break;
	case 0xa: /* lime-protection */
		/* lime-protection bug, value contains valve setting */
		if (l == 0xa || l == 0xb)
			break;
		/* else l == 0x2 or l == 0x3 */
		return -EINVAL;
		/* TBD: submit lime-protection */
		break;
	case 0xe: /* TEST */
		return -EINVAL;
		break;
	case 0xf: /* pair */
		return -EINVAL;
		break;
	}

	snprintf(dst, len, "%0.1f", (float)valve * 100 / 255);

	return 0;
}

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
		.input_conversion = input_not_accepted,
		.output_conversion = fht_is_temp_low,
	},
	/* is temp high */ {
		.function_id = FHT_IS_TEMP_HIGH,
		.name = "is-temp",
		.input_conversion = input_not_accepted,
		.output_conversion = fht_is_temp_high_to_str,
	},
	/* manu temp */ {
		.function_id = FHT_MANU_TEMP,
		.name = "manu-temp",
		.input_conversion = payload_to_fht_temp,
		.output_conversion = fht_temp_to_str,
	},
	/* day temp */ {
		.function_id = FHT_DAY_TEMP,
		.name = "day-temp",
		.input_conversion = payload_to_fht_temp,
		.output_conversion = fht_temp_to_str,
	},
	/* night temp */ {
		.function_id = FHT_NIGHT_TEMP,
		.name = "night-temp",
		.input_conversion = payload_to_fht_temp,
		.output_conversion = fht_temp_to_str,
	},
	/* window open temp */ {
		.function_id = FHT_WINDOW_OPEN_TEMP,
		.name = "window-open-temp",
		.input_conversion = payload_to_fht_temp,
		.output_conversion = fht_temp_to_str,
	},
};

int fht_decode(const struct payload *payload, struct fht_message *message)
{
	const static unsigned char magic_ack[] = {0x83, 0x09, 0x83, 0x01};
	const static unsigned char magic_status[] = {0x09, 0x09, 0xa0, 0x01};
	const struct fht_command *fht_command;
	struct fht_message_raw fht_message_raw = {0, 0, 0};
	unsigned char cmd;
	int i;

	if (payload->len < 9)
		return -EINVAL;

	if (!memcmp(payload->data, magic_ack, sizeof(magic_ack))) {
		message->type = ACK;
		cmd = payload->data[6];
		fht_message_raw.value = payload->data[7];
	} else if (!memcmp(payload->data, magic_status, sizeof(magic_status))) {
		if (payload->len != 10)
			return -EINVAL;
		message->type = STATUS;
		cmd = payload->data[6];
		fht_message_raw.subfun = payload->data[7];
		fht_message_raw.status = payload->data[8];
		fht_message_raw.value = payload->data[9];
	} else
		return -EINVAL;

	message->hauscode = *(const struct hauscode*)(payload->data + 4);
	message->topic2 = NULL;

	if (cmd == FHT_STATUS) {
		message->topic1 = "window";
		snprintf(message->value1, sizeof(message->value1), "%s",
			 fht_message_raw.value & (1 << 5) ? "open" : "close");
		message->topic2 = "battery";
		snprintf(message->value2, sizeof(message->value2), "%s",
			 fht_message_raw.value & (1 << 0) ? "empty" : "ok");
		return 0;
	}

	for_each_fht_command(fht_commands, fht_command, i) {
		if (fht_command->function_id != cmd)
			continue;
		message->topic1 = fht_command->name;
		return fht_command->output_conversion(message->value1,
					              sizeof(message->value1),
						      &fht_message_raw);
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
		if (fht_command->name && !strcmp(fht_command->name, command)) {
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
