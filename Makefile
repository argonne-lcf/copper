SRC := $(wildcard src/*.c)
OBJ := $(patsubst %.c,%.o, $(SRC))
CFLAGS := -g $(shell pkg-config fuse3 --cflags)
CFLAGS += -D_FILE_OFFSET_BITS=64
LDFLAGS := -g $(shell pkg-config fuse3 --libs)

all: cufuse

cufuse: $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

clean:
	rm -f $(OBJ) cufuse
