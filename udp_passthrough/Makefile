CC=gcc
LIBS=`pkg-config gstreamer-0.10 --libs`
CFLAGS=-Wall `pkg-config gstreamer-0.10 --cflags`

targets = client server

all: $(targets)

client: client.c
	$(CC) $(LIBS) $(CFLAGS) -o client client.c

server: server.c
	$(CC) $(LIBS) $(CFLAGS) -o server server.c

clean:
	rm $(targets)
