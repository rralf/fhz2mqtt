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

struct fhz_decoded;
struct mosquitto;

int mqtt_init(struct mosquitto **handle, const char *host, int port,
	      const char *username, const char *password);

void mqtt_close(struct mosquitto *mosquitto);
int mqtt_handle(struct mosquitto *mosquitto);
int mqtt_publish(struct mosquitto *mosquitto,
		 const struct fhz_decoded *decoded);
