server:	server.o threadpool.o
	gcc -o server server.o threadpool.o -Wall -lpthread

server.o: server.c threadpool.h
	gcc -c server.c

threadpool.o: threadpool.c threadpool.h
	gcc -c threadpool.c -lpthread
