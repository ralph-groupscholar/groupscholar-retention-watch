CC=clang
CFLAGS=-std=c11 -O2 -Wall -Wextra -pedantic
TARGET=retention-watch
SRC=src/main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)
