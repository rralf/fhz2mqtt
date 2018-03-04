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
#include "fhz.h"

#define S_FHZ "fhz/"
#define S_FHT "fht/"
#define S_SET "set/"

#define TOPIC "/" S_FHZ
#define TOPIC_SUBSCRIBE TOPIC S_SET
#define TOPIC_FHT TOPIC S_FHT

static int mqtt_subscribe(struct mosquitto *mosquitto)
{
	return mosquitto_subscribe(mosquitto, NULL, TOPIC_SUBSCRIBE "#", 0);
}

static int mqtt_receive_fht(int fd, const char *topic, const char *payload)
{
	struct hauscode hauscode;
	char buffer[5];

	if (strlen(topic) < 6)
		return -EINVAL;

	memcpy(buffer, topic, 4);
	buffer[4] = 0;
	if (hauscode_from_string(buffer, &hauscode))
		return -EINVAL;

	if (topic[4] != '/')
		return -EINVAL;

	topic += 5;

	return fht_set(fd, &hauscode, topic, payload);
}

static void callback(struct mosquitto *mosquitto, void *v_fd,
		     const struct mosquitto_message *message)
{
	const char *topic = message->topic + sizeof(TOPIC_SUBSCRIBE) - 1;
	char buffer[128];
	const int fd = (int)(size_t)v_fd;
	int err;

	if (message->payloadlen > 127)
		return;

	memcpy(buffer, message->payload, message->payloadlen);
	buffer[message->payloadlen] = 0;

	if (!strncmp(topic, S_FHT, sizeof(S_FHT) - 1)) {
		err = mqtt_receive_fht(fd, topic + sizeof(S_FHT) - 1, buffer);
	} else {
		err = -EINVAL;
	}

	if (err)
		printf("Unable to parse request: %s\n", strerror(-err));
}

static int mqtt_publish_fht(struct mosquitto *mosquitto, const struct fht_decoded *decoded)
{
	char topic[64];
	const char *type;

	type = decoded->type == ACK ? "ack" : "status";

	snprintf(topic, sizeof(topic), TOPIC_FHT "%02u%02u/%s/%s",
			 decoded->hauscode.upper, decoded->hauscode.lower,
			 type, decoded->topic1);

#ifdef DEBUG
	printf("%s: %s\n", topic, decoded->value1);
#endif
#ifndef NO_SEND
	mosquitto_publish(mosquitto, NULL, topic, strlen(decoded->value1),
			  decoded->value1, 0, false);
#endif

	return 0;
}

int mqtt_publish(struct mosquitto *mosquitto, const struct fhz_decoded *decoded)
{
	switch (decoded->machine) {
	case FHT:
		return mqtt_publish_fht(mosquitto, &decoded->fht);
	default:
		return -EINVAL;
	}
}

int mqtt_handle(struct mosquitto *mosquitto)
{
	int err;

	err = mosquitto_loop(mosquitto, 0, 1);
	if (err == MOSQ_ERR_CONN_LOST || err == MOSQ_ERR_NO_CONN) {
		err = mosquitto_reconnect(mosquitto);
		if (!err)
			err = mqtt_subscribe(mosquitto);
	}
	switch (err) {
	case MOSQ_ERR_SUCCESS:
		break;
	case MOSQ_ERR_CONN_LOST:
		return -ECONNABORTED;
	case MOSQ_ERR_NO_CONN:
		return -ECANCELED;
	case MOSQ_ERR_ERRNO:
		return -errno;
	default:
		return -EINVAL;
	}

	return 0;
}

int mqtt_init(struct mosquitto **handle, int fd, const char *host, int port,
	      const char *username, const char *password)
{
	struct mosquitto *mosquitto;
	int err;

	if (!host || !port)
		return -EINVAL;

	if (mosquitto_lib_init() != MOSQ_ERR_SUCCESS)
		return -EINVAL;

	mosquitto = mosquitto_new(NULL, true, (void*)(size_t)fd);
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

	err = mqtt_subscribe(mosquitto);
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
