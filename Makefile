#
# fhz2mqtt, a FHZ to MQTT bridge
#
# Copyright (c) Ralf Ramsauer, 2018
#
# Authors:
#  Ralf Ramsauer <ralf@ramses-pyramidenbau.de>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

OBJS = fhz.o fht.o mqtt.o main.o

CFLAGS := -ggdb -O0 -Wall -Wstrict-prototypes -Wmissing-prototypes -Werror

CFLAGS += -DDEBUG
# CFLAGS += -DNO_SEND

all: fhz2mqtt

fhz2mqtt: $(OBJS)
	$(CC) $(CFLAGS) -lmosquitto -o $@ $^

clean:
	rm -fv $(OBJS)
	rm -fv fhz2mqtt

test: fhz2mqtt
	./fhz2mqtt /dev/ttyUSB0 9601 127.0.0.1 1883
