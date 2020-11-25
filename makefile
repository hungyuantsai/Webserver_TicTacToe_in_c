all:
	gcc -g -lm server.c -o server
	gcc -g -lm client.c -o client
