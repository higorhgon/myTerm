CC = gcc
CFLAGS = $(shell pkg-config --cflags sdl3)
LIBS = $(shell pkg-config --libs sdl3) -lutil

all: myTerm

myTerm: myTerm.c
	mkdir build && $(CC) $(CFLAGS) myTerm.c -o build/myTerm $(LIBS)

clean: 
	rm -rf build
