OBJS = fhz.o fht.o mqtt.o main.o

CFLAGS := -ggdb -O0 -Wall -Wstrict-prototypes -Wmissing-prototypes -Werror

CFLAGS += -DDEBUG -DNO_SEND

all: fhz2mqtt

fhz2mqtt: $(OBJS)
	$(CC) $(CFLAGS) -lmosquitto -o $@ $^

clean:
	rm -fv $(OBJS)
	rm -fv fhz2mqtt

test: fhz2mqtt
	./fhz2mqtt /dev/ttyUSB0 9601 127.0.0.1 1883
