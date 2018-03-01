#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "fhz.h"

#define FHZ_MAGIC 0x81

#define BAUDRATE B9600

#ifdef DEBUG
static inline void hexdump(const unsigned char *data, size_t length)
{
	int __i;
	for (__i = 0; __i < length; __i++)
		printf("%02X ", data[__i]);
	printf("\n");
}
#else
#define hexdump(...)
#endif

int fhz_handle(int fd)
{
	struct payload payload;
	int err;

	err = fhz_decode(fd, &payload);
	if (err == -EAGAIN)
		return 0;

	if (err)
		return err;

	/* TBD handle payload */

	return err;
}

int fhz_send(int fd, const struct payload *payload)
{
	unsigned char buffer[256-2];
	unsigned char bc;
	int i, ret;

	bc = 0;
	for (i = 0; i < payload->len; i++)
		bc += payload->data[i];
	

	buffer[0] = FHZ_MAGIC;
	buffer[1] = payload->len + 2;
	buffer[2] = payload->tt;
	buffer[3] = bc;
	memcpy(buffer + 4, payload->data, payload->len);

	hexdump(buffer, payload->len + 4);

#ifndef NOSEND
	ret = write(fd, buffer, payload->len + 4);
	if (ret != payload->len + 4) {
		fprintf(stderr, "Error sending FHZ sequence\n");
		return -EINVAL;
	}
#endif

	return 0;
}

int fhz_decode(int fd, struct payload *payload)
{
	unsigned char buffer[256 + 2];
	unsigned char *payload_data;
	unsigned char payload_len;
	const int maxfd = fd + 1;
	unsigned char bc; /* the dump checksum */
	fd_set readset;
	ssize_t length;
	int ret;
	int i;

	FD_ZERO(&readset);
	FD_SET(fd, &readset);

	errno = EAGAIN;
	ret = select(maxfd, &readset, NULL, NULL, NULL);
	if (ret <= 0) /* on error and timeout */
		return -errno;

	/* on receive, just wait a bit... */
	usleep(300000);

	length = read(fd, buffer, 2);
	if (length < 2) {
		if (length == -1) {
			error("Read from serial fail: %s\n", strerror(errno));
			return -errno;
		} else {
			error("Packet shorter than four bytes: %zd\n", length);
			return -EINVAL;
		}
	}

	if (buffer[0] != FHZ_MAGIC) {
		fprintf(stderr, "Invalid packet magic\n");
		return -EINVAL;
	}

	length = read(fd, buffer + 2, buffer[1]);
	if (length < buffer[1]) {
		if (length == -1) {
			error("Read from serial fail: %s\n", strerror(errno));
			return -errno;
		} else {
			error("Packet shorter expected: got %zd, expected %u\n", length, buffer[1]);
			return -EINVAL;
		}
	}

	length += 2;

	hexdump(buffer, length);

	if (length < 4) {
		fprintf(stderr, "Packet misses type or crc\n");
		return -EINVAL;
	}

	payload_data = buffer + 4;
	payload_len = length - 4;

	bc = 0;
	for (i = 0; i < payload_len; i++)
		bc += payload_data[i];

	if (bc != buffer[3]) {
		fprintf(stderr, "Packet checksum mismatch\n");
		return -EINVAL;
	}

	payload->tt = buffer[2];
	memcpy(payload->data, payload_data, payload_len);
	payload->len = payload_len;

	return 0;
}

int fhz_open_serial(const char *device)
{
	struct termios tty;
	int err, fd;

        fd = open(device, O_RDWR | O_NOCTTY);
        if (fd == -1) {
		error("opening %s: %s\n", device, strerror(errno));
		return -errno;
	}

        memset(&tty, 0, sizeof tty);
        err = tcgetattr (fd, &tty);
	if (err) {
		error("tcgetattr: %s\n", strerror(errno));
		goto close_out;
	}

        err = cfsetospeed (&tty, BAUDRATE);
	if (err) {
		error("cfsetospeed: %s\n", strerror(errno));
		goto close_out;
	}

        err = cfsetispeed(&tty, BAUDRATE);
	if (err) {
		error("cfsetispeed: %s\n", strerror(errno));
		goto close_out;
	}

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
        tty.c_iflag &= ~IGNBRK;
        tty.c_lflag = 0;
        tty.c_oflag = 0;
        tty.c_cc[VMIN]  = 0;
        tty.c_cc[VTIME] = 10;

        tty.c_iflag &= ~(IXON | IXOFF | IXANY);

        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cflag &= ~(PARENB | PARODD);
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        err = tcsetattr (fd, TCSANOW, &tty);
	if (err) {
		error("tcsetattr: %s", strerror(errno));
		goto close_out;

	}

	return fd;

close_out:
	close(fd);
	return -errno;
}
