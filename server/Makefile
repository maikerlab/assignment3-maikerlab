CC := $(CROSS_COMPILE)gcc
CFLAGS := -g -Wall -Werror -D_POSIX_C_SOURCE=200809L
TARGET := aesdsocket
SRC := aesdsocket.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o
