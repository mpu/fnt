CFLAGS += -Wall

all: edit view

clean:
	rm -f edit view

edit: edit.c
	$(CC) $(CFLAGS) -lncurses -o $@ $^

view: view.c
	$(CC) $(CFLAGS) -lX11 -o $@ $^

.PHONY: all clean
