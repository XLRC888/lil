CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
TARGET = lil

$(TARGET): lil.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

test: $(TARGET)
	./$(TARGET) test.lil

repl: $(TARGET)
	./$(TARGET)

.PHONY: clean test repl
