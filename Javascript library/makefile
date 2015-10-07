TARGET = eph5.js
OBJS = main.o

PRE = pre.js
POST = post.js

CC = emcc
export CPATH = .:jpeg:eph5
CFLAGS += -std=c99
LDFLAGS += --memory-init-file 0 -s MEM_INIT_METHOD=2 -s NO_FILESYSTEM=1 -s NO_BROWSER=1 -s NO_DYNAMIC_EXECUTION=1 -s NO_EXIT_RUNTIME=1 -s EXPORT_NAME=\"kernel\" -s "EXPORTED_FUNCTIONS=[\"_realloc\", \"_main\", \"_export_globals\", \"_load\", \"_save\", \"_destroy\", \"_embed\", \"_reset\", \"_extract\"]"
LDLIBS += eph5/libeph5.a nettle/libnettle.a jpeg/.libs/libjpeg.a

$(TARGET): $(OBJS) $(PRE) $(POST)
	$(CC) $(LDFLAGS) --pre-js $(PRE) --post-js $(POST) $(OBJS) $(LDLIBS) -o $@

clean:
	rm -f $(TARGET) $(OBJS)
