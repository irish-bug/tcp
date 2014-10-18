#ifndef SENDER_H
#define SENDER_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <fstream>
#include <string>
#include <deque>
using namespace std;

#define MAX_DATA_SIZE 1024 //packets will be 1KB

class Socket {
private:
	struct addrinfo *p;
	int sockfd;
public:
	void setSockFD(int fd);
	int getSockFD();
	int setAddrInfo(struct addrinfo *info);
	struct addrinfo * getAddrInfo();
};

class Packet {
private:
	char data[MAX_DATA_SIZE];
	unsigned long long int sequence_num;
	unsigned int packet_len; // number of bytes in packet
public:
	void setSequenceNum(unsigned long long int num);
	unsigned long long int getSequenceNum();
	int setPacketData(char * buf, unsigned int size);
	void getPacketData(char * buf);
	unsigned int getPacketSize();
};

class CongestionWindow {
private:
	unsigned long long int lowest_seq_num;
	unsigned long long int last_ACK;
	int RTO;
	int window_size;
	int DUPACK_counter;
	deque<Packet> window;
public:
	void setLowestSeqNum(unsigned long long int new_num);
	unsigned long long int getLowestSeqNum();
	void setLastACK(unsigned long long int ACK_num);
	unsigned long long int getLastACK();
	void setNewRTO(int timeout);
	void setWindowSize(int size);
	int getWindowSize();
	void setDUPACKcounter(int val);
	int getDUPACKcounter();
	void incrementDUPACKcounter();
	int getNumPktsToAdd();
	// add growth schemes here

	void addPacket(char * buf, unsigned int size, int sockfd, struct addrinfo * p);
	void removePackets(int n); // pop n packets off the queue
	void sendWindow(int sockfd, struct addrinfo * p); // send all the packets in CW
	void cutWindow(); // reduce window size on three DUPACKS
	void panicMode(); // set CW back to 1 on timeout
};

#endif