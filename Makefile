CFLAGS = -Wall

CC = gcc

all: my_utils.o client server

my_utils.o: my_utils.c my_utils.h
	${CC} ${CFLAGS} -c my_utils.c

#uthash.o: uthash-1.9.7/src/uthash.c
#	${CC} ${CFLAGS} -c uthash-1.9.7/src/uthash.c

client: my_utils.o client.o 
	${CC} ${CFLAGS} -pthread -o $@ client.o 

server: my_utils.o server.o
	${CC} ${CFLAGS} -pthread -o $@ my_utils.o server.o

test: test.o
	${CC} ${CFLAGS} -o $@ test.o

clean:
	rm *.o client server
