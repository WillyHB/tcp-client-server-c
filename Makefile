all: client server

server: server.o database.c
	gcc -o server server.o 

server.o: server.c database.c
	gcc -c server.c

client: client.o
	gcc -o client client.o 

client.o: client.c 
	gcc -c client.c
