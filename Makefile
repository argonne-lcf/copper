SRC := $(wildcard src/*.c)
OBJ := $(patsubst %.c,%.o, $(SRC))
CFLAGS := -g 
LDFLAGS := -g

all: cufuse

cufuse: $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $^

clean:
	rm -f $(OBJ) cufuse
