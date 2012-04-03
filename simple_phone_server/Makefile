CC=gcc
LIBS=`pkg-config gstreamer-0.10 --libs`
CFLAGS=-Wall `pkg-config gstreamer-0.10 --cflags`

main: main.c
	$(CC) $(LIBS) $(CFLAGS) -o phone_server main.c

clean:
	rm phone_server

remake: clean main
