CC=gcc
CFLAGS=-Wall -g -std=c11 -lpthread

all: A2checker

A2checker: main.c
	$(CC) -o A2checker main.c $(CFLAGS)

clean:
	rm -f A2checker *.out
