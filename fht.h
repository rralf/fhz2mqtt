#include <ctype.h>
#include <errno.h>
#include <string.h>

struct hauscode {
	unsigned char upper;
	unsigned char lower;
} __attribute__((packed));

static inline int hauscode_from_string(const char *string,
				       struct hauscode *hauscode)
{
	int i;

	if (strlen(string) != 4)
		return -EINVAL;

	for (i = 0; i < 4; i++)
		if (!isdigit(string[i]))
			return -EINVAL;

	hauscode->upper = (string[0] - '0') * 10;
	hauscode->upper += (string[1] - '0') * 1;
	hauscode->lower = (string[2] - '0') * 10;
	hauscode->lower += (string[3] - '0') * 1;

	return 0;
}

int fht80b_set_temp(int fd, struct hauscode *hauscode, float temp);
