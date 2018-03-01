OBJS = fhz.o fht.o mqtt.o main.o

CFLAGS := -ggdb -O0 -Wall -Wstrict-prototypes -Wmissing-prototypes -Werror

CFLAGS += -DDEBUG -DNO_SEND

all: fhz2mqtt

fhz2mqtt: $(OBJS)
	$(CC) $(CFLAGS) -lmosquitto -o $@ $^

clean:
	rm -fv $(OBJS)
	rm -fv fhz2mqtt