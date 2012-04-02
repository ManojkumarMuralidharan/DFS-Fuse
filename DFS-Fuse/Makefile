#
#
CC=gcc
CFLAGS=-g -D_FILE_OFFSET_BITS=64

# comment line below for Linux machines
LIB= -lpthread -lfuse

all: server client password

server:	server.o
	$(CC) $(CFLAGS) -o $@ serverDFS.c $(LIB)

client:	client.o
	$(CC) $(CFLAGS) -o $@ clientDFS.c $(LIB)

server.o:	serverDFS.c

client.o:	clientDFS.c 

clean:
	\rm -f server client server.o client.o

squeaky:
	make clean
	\rm -f server.o client.o

password:
	echo "Kedar mishra">passwords
	mkdir /tmp/client
	mkdir /tmp/client/Kedar


