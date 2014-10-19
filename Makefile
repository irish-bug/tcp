#
# Machine Problem #2
# CS 438
#

CC = g++
INC = -I.
FLAGS = -std=c++0x -g -Wall
LIBS = -lpthread

all: reliable_sender reliable_receiver

sender.o: sender.cpp sender.h
	$(CC) -c $(FLAGS) $(INC) $< -o $@ $(LIBS)

receiver_main.o: receiver_main.cpp
	$(CC) -c $(FLAGS) $(INC) $< -o $@ $(LIBS)

sender_main.o: sender_main.cpp
	$(CC) -c $(FLAGS) $(INC) $< -o $@ $(LIBS)

sender: sender.o
	$(CC) $^ -o $@ $(LIBS)

reliable_receiver: receiver_main.o
	$(CC) $^ -o $@ $(LIBS)

reliable_sender: sender_main.o sender.o
	$(CC) $^ -o $@ $(LIBS)

.PHONY : clean
clean:
	-rm -r -f *.o reliable*
