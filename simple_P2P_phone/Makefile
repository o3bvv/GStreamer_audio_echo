CC=gcc
LIBS=`pkg-config gstreamer-0.10 --libs`
CFLAGS=-Wall `pkg-config gstreamer-0.10 --cflags`

main: main.c
	$(CC) $(LIBS) $(CFLAGS) -o simple_phone main.c

clean:
	rm simple_phone

remake: clean main
