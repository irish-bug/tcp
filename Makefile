#
# Machine Problem #1
# CS 438
#

CC = gcc
INC = -I.
FLAGS = -g 

all: beej abctalk abc

beej: beejtalk beejlisten

abc: abctalk abclisten

beejtalk: talker.o
	$(CC) $^ -o $@

beejlisten: listener.o
	$(CC) $^ -o $@	

abctalk: server.o client.o
	$(CC) $^ -o $@

abclisten: client.o
	$(CC) $^ -o $@	

client.o: udp_client.c
	$(CC) -c $(FLAGS) $(INC) $< -o $@

server.o: udp_server.c
	$(CC) -c $(FLAGS) $(INC) $< -o $@

listener.o: listener.c
	$(CC) -c $(FLAGS) $(INC) $< -o $@

talker.o: talker.c
	$(CC) -c $(FLAGS) $(INC) $< -o $@

.PHONY : clean
clean:
	-rm -r -f *.o beejlisten beejtalk abclisten abctalk