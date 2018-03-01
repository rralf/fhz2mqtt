#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "fhz.h"
#include "fht.h"

static void __attribute__((noreturn)) usage(int code)
{
	printf("Usage: fht2mqtt usb_port\n");
	exit(code);
}

int main(int argc, const char **argv)
{
	struct payload payload;
	int err, fd;

	if (argc != 2)
		usage(-EINVAL);

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

	close(fd);
	return err;
}
