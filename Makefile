CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lSDL2 -lSDL2_ttf -lm
TARGET = main
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean