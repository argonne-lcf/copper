SRC := $(wildcard src/*.c)
OBJ := $(patsubst %.c,%.o, $(SRC))
CFLAGS := -g $(shell pkg-config fuse3 --cflags)
LDFLAGS := -g $(shell pkg-config fuse3 --libs)

all: cufuse

cufuse: $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

clean:
	rm -f $(OBJ) cufuse
