CC=gcc 
CFLAGS=-std=gnu11 -g -Wall -Wextra

all: main malloc.so

main:  malloc.o main.o
	$(CC) $(CFLAGS) -o main main.o malloc.o  -lpthread
main.o: main.c 
	$(CC) $(CFLAGS) -c main.c -o main.o

malloc.so: malloc.o
	gcc -shared -o malloc.so malloc.o
malloc.o: malloc.c malloc.h
	$(CC) $(CFLAGS) -fpic -c malloc.c -o malloc.o

clean: 
	rm -vf main *.o malloc.so
