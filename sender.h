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

#define MAX_DATA_SIZE 1024 //packets will be 1KBm

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
	unsigned long long int lowest_seq_num; // lowest sequence number in the congestion window
	unsigned long long int highest_seq_num; // highest sequence number in the congestion window
	unsigned long long int last_ACK; // sequence number of the lastACKd packet
	int window_size; // size of the congestion window
	deque<Packet> window; // deque of packets, actual congestion window
public:
	void setLowestSeqNum(unsigned long long int new_num);
	unsigned long long int getLowestSeqNum();
	void setHighestSeqNum(unsigned long long int new_num);
	unsigned long long int getHighestSeqNum();
	void setLastACK(unsigned long long int ACK_num);
	unsigned long long int getLastACK();
	void setWindowSize(int size);
	int getWindowSize();
	int getNumPktsToAdd();
	void addPacket(char * buf, unsigned int size, unsigned long long int seqnum, int sockfd, struct addrinfo * p);
	void removePackets(int n); // pop n packets off the queue
	void cutWindow(); // reduce window size on three DUPACKS
	void panicMode(); // set CW back to 1 on timeout
};

#endif