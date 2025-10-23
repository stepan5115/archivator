CC=gcc
SRCS=arch.c
TARGET=arch

all:
	$(CC) -o $(TARGET) $(SRCS)
clean:
	rm -f $(TARGET)
