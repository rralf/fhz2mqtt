#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "fhz.h"
#include "fht.h"
#include "mqtt.h"

static void __attribute__((noreturn)) usage(int code)
{
	printf("Usage: fht2mqtt usb_port fht_hauscode mqtt_server mqtt_port [username] [password]\n");
	exit(code);
}

int main(int argc, const char **argv)
{
	struct payload payload;
	int err, fd;
	struct mosquitto *mosquitto;
	const char *username = NULL, *password = NULL;
	unsigned int port;

	if (argc < 5)
		usage(-EINVAL);

	if (argc == 7) {
		username = argv[5];
		password = argv[6];
	}

	port = strtoul(argv[4], NULL, 10);

	err = mqtt_init(&mosquitto, argv[3], port, username, password);
	if (err) {
		fprintf(stderr, "MQTT connection failure\n");
		return -1;
	}

	fd = fhz_open_serial(argv[1]);
	if (fd < 0)
		return fd;

	struct hauscode hauscode;
	hauscode_from_string("9601", &hauscode);

	err = fht80b_set_temp(fd, &hauscode, 17);
	if (err)
		return -EINVAL;

	do {
		err = fhz_decode(fd, &payload);
		if (err) {
			if (err != -EAGAIN)
				error("Error decoding packet: %s\n", strerror(-err));
			continue;
		}
	} while(true);

	err = 0;

	mqtt_close(mosquitto);
	close(fd);
	return err;
}
