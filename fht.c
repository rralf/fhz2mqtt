#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

#include "fhz.h"
#include "fht.h"

#if 0
const static struct payload payload_hello = {
	.tt = 0xc9,
	.len = 4,
	.data = {0x02, 0x01, 0x1f, 0x60},
};

const static struct payload payload_status_serial = {
	.tt = 0x04,
	.len = 6,
	.data = {0xc9, 0x01, 0x84, 0x57, 0x02, 0x08},
};
#endif

int fht80b_set_temp(int fd, struct hauscode *hauscode, float temp)
{
	int x = (int)(temp / 0.5);
	const struct payload payload = {
		.tt = 0x04,
		.len = 7,
		.data = {0x02, 0x01, 0x83, hauscode->upper, hauscode->lower, 0x41, x},
	};

	return fhz_send(fd, &payload);
}
