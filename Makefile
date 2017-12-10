all: server

server: server.c
	gcc -O3 -g -std=gnu11 -o server server.c

