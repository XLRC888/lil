CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
TARGET = lil
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0 2>/dev/null)

$(TARGET): lil.c
	$(CC) $(CFLAGS) -o $@ $<

gtk: lil.c
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -DHAVE_GTK -o $(TARGET) $< $(GTK_LIBS)

clean:
	rm -f $(TARGET)

test: $(TARGET)
	./$(TARGET) test.lil

repl: $(TARGET)
	./$(TARGET)

.PHONY: clean test repl gtk
