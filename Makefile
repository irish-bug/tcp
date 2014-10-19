#
# Machine Problem #2
# CS 438
#

CC = g++
INC = -I.
FLAGS = -std=c++0x -g -Wall
LIBS = -lpthread

all: reliably_send reliably_receive

sender.o: sender.cpp sender.h
	$(CC) -c $(FLAGS) $(INC) $< -o $@ $(LIBS)

reliably_receive.o: reliably_receive.cpp
	$(CC) -c $(FLAGS) $(INC) $< -o $@ $(LIBS)

reliably_send.o: reliably_send.cpp
	$(CC) -c $(FLAGS) $(INC) $< -o $@ $(LIBS)

sender: sender.o
	$(CC) $^ -o $@ $(LIBS)

reliably_receive: reliably_receive.o
	$(CC) $^ -o $@ $(LIBS)

reliably_send: reliably_send.o sender.o
	$(CC) $^ -o $@ $(LIBS)

.PHONY : clean
clean:
	-rm -r -f *.o sender reliably_send reliably_send
