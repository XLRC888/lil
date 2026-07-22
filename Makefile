CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
TARGET = lil
TARGET_GTK = lil-gtk

PREFIX ?= $(HOME)/.local
BINDIR = $(PREFIX)/bin
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0 2>/dev/null)

SRC = src/main.c src/lexer.c src/parser.c src/libs.c src/vm.c src/transpiler.c src/java_gen.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -Isrc -o $@ $^ -lm

gtk: $(SRC) src/gtk.c
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -DHAVE_GTK -Isrc -o $(TARGET_GTK) $(SRC) src/gtk.c $(GTK_LIBS) -lm



clean:
	rm -f $(TARGET) $(TARGET_GTK)

install: $(TARGET)
	mkdir -p $(BINDIR)
	cp $(TARGET) $(BINDIR)/$(TARGET)

install-gtk: gtk
	mkdir -p $(BINDIR)
	cp $(TARGET_GTK) $(BINDIR)/$(TARGET)



test: $(TARGET)
	./$(TARGET) test.lil

repl: $(TARGET)
	./$(TARGET)

.PHONY: clean test repl gtk install-gtk
