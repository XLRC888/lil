CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
TARGET = lil
TARGET_GTK = lil-gtk
TARGET_WL = lil-wlroots
PREFIX ?= $(HOME)/.local
BINDIR = $(PREFIX)/bin
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0 2>/dev/null)
WLROOTS_CFLAGS = $(shell pkg-config --cflags wlroots-0.21 wayland-server xkbcommon 2>/dev/null) -DWLR_USE_UNSTABLE
WLROOTS_LIBS = $(shell pkg-config --libs wlroots-0.21 wayland-server xkbcommon 2>/dev/null)
SRC = src/main.c src/lexer.c src/parser.c src/libs.c src/vm.c src/transpiler.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -Isrc -o $@ $^ -lm

gtk: $(SRC) src/gtk.c
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -DHAVE_GTK -Isrc -o $(TARGET_GTK) $(SRC) src/gtk.c $(GTK_LIBS) -lm

wlroots: $(SRC) src/wlroots.c
	$(CC) $(CFLAGS) $(WLROOTS_CFLAGS) -DHAVE_WLROOTS -Isrc -o $(TARGET_WL) $(SRC) src/wlroots.c $(WLROOTS_LIBS) -lm

clean:
	rm -f $(TARGET)

install: $(TARGET)
	mkdir -p $(BINDIR)
	cp $(TARGET) $(BINDIR)/$(TARGET)

install-gtk: gtk
	mkdir -p $(BINDIR)
	cp $(TARGET_GTK) $(BINDIR)/$(TARGET)

install-wlroots: wlroots
	mkdir -p $(BINDIR)
	cp $(TARGET_WL) $(BINDIR)/$(TARGET)

test: $(TARGET)
	./$(TARGET) test.lil

repl: $(TARGET)
	./$(TARGET)

.PHONY: clean test repl gtk install-gtk wlroots install-wlroots
