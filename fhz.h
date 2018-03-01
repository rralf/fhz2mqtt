#define FHZ_MAGIC 0x81

#define BAUDRATE B9600

#define error(...) \
	do { \
		char error_buffer[128]; \
		snprintf(error_buffer, sizeof(error_buffer), __VA_ARGS__);  \
		fprintf(stderr, error_buffer); \
	} while(0)

struct payload {
	unsigned char tt;
	unsigned char len;
	unsigned char data[256];
};

int fhz_open_serial(const char *device);
int fhz_send(int fd, const struct payload *payload);
int fhz_decode(int fd, struct payload *payload);
