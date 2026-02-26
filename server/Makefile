CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -Werror -std=c11 -D_POSIX_C_SOURCE=200809L
TARGET := aesdsocket
SRC := aesdsocket.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o
