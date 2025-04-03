CC = gcc
CFLAGS = -Wall -Wextra -g
#LDFLAGS = -lSDL3 -lSDL3_ttf -lm
LDFLAGS = -lSDL3 -lm
TARGET = main
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
