CC=gcc
SRSC=arch.c
TARGET=arch

all:
	$(CC) -o $(TARGET) $(SCRC)
clean:
	rm -f $(TARGET)
