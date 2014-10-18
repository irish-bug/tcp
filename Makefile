#
# Machine Problem #1
# CS 438
#

CC = g++
INC = -I.
FLAGS = -pthread -std=c++0x -g -Wall

all: sender_main receiver_main

sender.o: sender.cpp sender.h
	$(CC) -c $(FLAGS) $(INC) $< -o $@

receiver_main.o: receiver_main.cpp
	$(CC) -c $(FLAGS) $(INC) $< -o $@

sender_main.o: sender_main.cpp
	$(CC) -c $(FLAGS) $(INC) $< -o $@

sender: sender.o
	$(CC) $^ -o $@

receiver_main: receiver_main.o
	$(CC) $^ -o $@

sender_main: sender_main.o sender.o
	$(CC) $^ -o $@

.PHONY : clean
clean:
	-rm -r -f *.o sender receiver_main sender_main
