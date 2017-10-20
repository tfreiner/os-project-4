CC=gcc
CFLAGS=-Wall -pthread
TARGET=oss user

all: $(TARGET) 

$(TARGET): %: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f *.o $(TARGET)
