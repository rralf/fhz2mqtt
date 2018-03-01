#include <errno.h>
#include <mosquitto.h>
#include <stddef.h>
#include <stdio.h>

#include "mqtt.h"

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
