#ifndef SENDER_H
#define SENDER_H

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
using namespace std;

#define MAX_DATA_SIZE 1024 //packets will be 1KB

class Packet {
private:
	char data[MAX_DATA_SIZE];
	int sequence_num;
public:
	Packet(int num, char * buf);
	void setSequenceNum(int num);
	void setPacketData(char * buf);
};

class CongestionWindow {
private:
	int lowest_seq_num;
	int last_ACK;
	int RTO;
	int window_size;
	vector<Packet> window;
public:
	CongestionWindow(int timeout);
	// add growth schemes here
	int slideWindow(int n);
	int removePackets(int n); // pop n packets off the queue
	void cutWindow(); // reduce window size on three DUPACKS
	void panicMode(); // set CW back to 1 on timeout
};

class Socket {
private:
	struct addrinfo *p;
	int sockfd;
public:
	int setSockFD(int fd);
	int setAddrInfo(struct addrinfo *info);
};

#endif