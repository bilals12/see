CC=gcc
CFLAGS=-Wall -framework ApplicationServices -lcurl -pthread
TARGET=see
SOURCES=see.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(SOURCES) -o $(TARGET) $(CFLAGS)

clean:
	rm -f $(TARGET)
	rm -f *.o

.PHONY: all clean