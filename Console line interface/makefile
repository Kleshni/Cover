TARGET = eph5
OBJS = main.o

CFLAGS += -std=c99
LDLIBS += -leph5 -ljpeg -lnettle

ifdef DEBUG
	CFLAGS += -g3 -Wpedantic -Wall -Wextra
else
	LDFLAGS += -s
endif

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -f $(TARGET) $(OBJS)
