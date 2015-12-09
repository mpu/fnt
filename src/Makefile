.PHONY: all clean

all: edit view

clean:
	rm -f edit view

edit: edit.c
	cc -lncurses -o edit edit.c

view: view.c
	cc -Wall -lX11 -o view view.c
