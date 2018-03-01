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
#include <mosquitto.h>
#include <stddef.h>
#include <stdio.h>

#include "mqtt.h"

static void callback(struct mosquitto *mosquitto, void *foo,
		     const struct mosquitto_message *message)
{
	printf("Callback!\n");
}

int mqtt_handle(struct mosquitto *mosquitto)
{
	int err;

	err = mosquitto_loop(mosquitto, 0, 1);
	return err;
}

int mqtt_init(struct mosquitto **handle, const char *host, int port,
	      const char *username, const char *password)
{
	struct mosquitto *mosquitto;
	int err;

	if (!host || !port)
		return -EINVAL;

	if (mosquitto_lib_init() != MOSQ_ERR_SUCCESS)
		return -EINVAL;

	mosquitto = mosquitto_new(NULL, true, NULL);
	if (!mosquitto)
		return -errno;

	if (username && password) {
		err = mosquitto_username_pw_set(mosquitto, username, password);
		if (err)
			goto close_out;
	}

	err = mosquitto_connect(mosquitto, host, port, 120);
	if (err) {
		fprintf(stderr, "mosquitto connect error\n");
		goto close_out;
	}

	err = mosquitto_subscribe(mosquitto, NULL, "/fhz/#", 0);
	if (err) {
		fprintf(stderr, "mosquitto subscription error\n");
	}

	mosquitto_message_callback_set(mosquitto, callback);

	*handle = mosquitto;
	return 0;

close_out:
	mosquitto_destroy(mosquitto);
	mosquitto_lib_cleanup();
	return -1;
}

void mqtt_close(struct mosquitto *mosquitto)
{
	mosquitto_destroy(mosquitto);
	mosquitto_lib_cleanup();
}
