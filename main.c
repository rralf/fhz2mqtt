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

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "fhz.h"
#include "mqtt.h"

#define MQTT_DEFAULT_PORT 1883
#define MQTT_DEFAULT_HOSTNAME "localhost"

static void __attribute__((noreturn)) usage(int code)
{
	printf("Usage: fht2mqtt usb_port"
	       "[mqtt_server] [mqtt_port] [username] [password]\n");
	exit(code);
}

int main(int argc, const char **argv)
{
	const char *username = NULL, *password = NULL;
	const char *hostname = MQTT_DEFAULT_HOSTNAME;
	unsigned int port = MQTT_DEFAULT_PORT;
	struct mosquitto *mosquitto;
	struct fhz_message message;
	int err, fd;

	if (argc < 4 && argc != 2)
		usage(-EINVAL);

	if (argc >= 3)
		hostname = argv[2];

	if (argc >= 4)
		port = strtoul(argv[3], NULL, 10);

	if (argc == 6) {
		username = argv[4];
		password = argv[5];
	}

	fd = fhz_open_serial(argv[1]);
	if (fd < 0)
		return fd;

	err = mqtt_init(&mosquitto, fd, hostname, port, username, password);
	if (err) {
		fprintf(stderr, "MQTT connection failure\n");
		goto close_out;
	}

	do {
		err = fhz_handle(fd, &message);
		if (err && err != -EAGAIN)
			error("Error decoding packet: %s\n", strerror(-err));
		else if (!err) {
			err = mqtt_publish(mosquitto, &message);
			if (err)
				fprintf(stderr, "mqtt: unable to publish FHZ "
						"message\n");
		}

		err = mqtt_handle(mosquitto);
		if (err)
			error("MQTT error: %s\n", strerror(-err));

		sleep(1);
	} while(true);

	err = 0;

	mqtt_close(mosquitto);
close_out:
	close(fd);
	return err;
}
