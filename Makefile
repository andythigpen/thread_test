
all: thread_test

thread_test: main.o
	gcc main.o -lpthread -lrt -o thread_test

main.o: main.c
	gcc -c main.c 
