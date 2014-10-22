#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <ctime>
#include <string>
#include <cstring>
#include <thread>
#include <mutex>
#include "sender.h"
using namespace std;

#define ACK "ACK"
#define SYN "SYN"
#define END "END\n"
#define ACK_SIZE 128
#define THRESHOLD 64 // TCP uses a threshold of 64KB
#define TIMEOUT 30 // in milliseconds

//unsigned long long int SEQ; // keeps track of SEQ for adding packets

int sockfd;
struct addrinfo * p;
//CongestionWindow cw;
//unsigned long long int bytes;
//char * file;

//unsigned long long int lastACK;
//unsigned int DUPACKctr;
//unsigned long long int bytesSent;

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer);

int main(int argc, char** argv)
{
	unsigned short int udpPort;
	unsigned long long int numBytes;
	
	if(argc != 5)
	{
		fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
		exit(1);
	}
	udpPort = (unsigned short int)atoi(argv[2]);
	numBytes = atoll(argv[4]);
	
	reliablyTransfer(argv[1], udpPort, argv[3], numBytes);
	return 0;
}

unsigned long long int getACKnum(char * msg) {
	// strip the ACK number from message
	unsigned long long int num;
	sscanf(msg, "%llu", &num);
	return num;
}

int initialize_TCP() {
	char buf[3]; //store 

	int numbytes;

	cout << "SYN -->\n";
	if ((numbytes = sendto(sockfd, SYN, 3, 0,
             p->ai_addr, p->ai_addrlen)) == -1) {
        perror("sender: sendto");
        exit(1);
    }
    
    if ((numbytes = recvfrom(sockfd, buf, 3, 0,
        	p->ai_addr, (socklen_t *)&p->ai_addrlen)) == -1) {
        perror("recvfrom");
        exit(1);
    }

    if (memcmp(buf,"ACK",3) != 0) {
    	cout << "Received: " << buf << ".\n";
    	return -1;
    }
    else {
    	cout << "ACK <--\n";
    }

	return 0; // return estimated timeout
}

void setup_UDP(char * hostname, unsigned short int port) {
	//int sockfd;
    struct addrinfo hints, *servinfo;//, *p;
    int rv;
    //int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    string port_str = to_string(port);
    if ((rv = getaddrinfo(hostname, port_str.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to bind socket\n");
        return;
    }
}


void sendPackets(char * filename, unsigned long long int bytesToTransfer, CongestionWindow cw) {
	unsigned long long int bytesRead = 0;
	unsigned long long int SEQ = 1;
	bool slowStart = true;
	int partialPacket = 0;
	int numbytes, timeout;
	//char buf[MAX_DATA_SIZE];
	int numPktsToAdd, numPktsToSend, index, packet_size;
	unsigned long long int lastACK = 0;
	unsigned int DUPACKctr = 0;
	unsigned long long int bytesSent = 0;
	struct timeval tv;


	ifstream myFile(filename);
	if(!myFile.is_open()) {
		cout << "reliablyTransfer: Unable to open file.\n";
		return;
	}

	// setup timeout on recv socket
	tv.tv_sec = 0;
	tv.tv_usec = 30000;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
	    perror("Error");
	}

	while (bytesSent < bytesToTransfer) {

		numPktsToAdd = cw.getNumPktsToAdd();

		if(numPktsToAdd < 0) {
			numPktsToAdd = 0;
		}

		char buf[MAX_DATA_SIZE];
		for(int i=0; i<numPktsToAdd; i++) {
			if (bytesRead >= bytesToTransfer) { break; }

			unsigned long long int diff = bytesToTransfer - bytesRead;
	        myFile.read(buf, min(diff,(unsigned long long int)MAX_DATA_SIZE));
	        bytesRead += min(diff,(unsigned long long int)MAX_DATA_SIZE);

	        if(diff > 0 && diff < MAX_DATA_SIZE) { 
	        	packet_size = diff; 
	        }
	        else { 
	        	packet_size = MAX_DATA_SIZE; 
	        }

			cw.addPacket(buf, packet_size, SEQ, sockfd, p); // this adds packets and sends them!
			cw.setHighestSeqNum(SEQ);
			SEQ++;
		}

		unsigned long long int lastSent = cw.getLastSent();
		unsigned long long int low = cw.getLowestSeqNum();
		numPktsToSend = (low + cw.getWindowSize()) - lastSent - 1;
		index = lastSent - low + 1;
		//cout << "index = " << index << endl;
		for(int i=index; i < cw.getWindowSize(); i++) {
			cw.sendPacket(i, sockfd, p);
		}

		// LISTEN FOR ACKs
		char resp[MAX_DATA_SIZE];
		timeout = 0;
		if ((numbytes = recvfrom(sockfd, resp, MAX_DATA_SIZE, 0,
	        	p->ai_addr, (socklen_t *)&p->ai_addrlen)) == -1) {
	    	if(errno == EAGAIN || errno == EWOULDBLOCK) {
	    		timeout = 1;
	    	}
	    	else {
	        	exit(1);
	        }
	    }

	    if(timeout) {
	    	cw.panicMode();
			slowStart = true;
	    }
	    else {
	    	// we got an ACK
	    	unsigned long long int ACKnum = getACKnum(resp);

	    	if (ACKnum == lastACK) { // DUPACK
	    		DUPACKctr++;
	    		bytesSent = ACKnum * MAX_DATA_SIZE; // the number of bytes successfully sent
	    		if (DUPACKctr >= 10) {
	    			cw.cutWindow();
					unsigned long long int newSEQ = cw.sendWindow(sockfd, p); // resend the window!
					SEQ = newSEQ + 1;
					DUPACKctr = 0;
					unsigned long long int new_low = lastACK + 1;
					cw.setLowestSeqNum(new_low);
	    		}
	    	}
	    	else if (ACKnum > lastACK) { // not a DUPACK!
	    		lastACK = ACKnum;
	    		DUPACKctr = 0;
	    		bytesSent = ACKnum * MAX_DATA_SIZE;
	    		int ws = cw.getWindowSize();
				cw.setLastACK(lastACK);
				int diff = lastACK - cw.getLowestSeqNum() + 1;
				cw.removePackets(diff); // pop off ACKd packets

				if(slowStart) {
					ws = cw.getWindowSize() + 1;
				}
				else{
					partialPacket++;
					if(partialPacket == cw.getWindowSize()) {
						ws = cw.getWindowSize() + 1;
					}
				}
				cw.setWindowSize(ws);
				if(ws >= THRESHOLD) {
					slowStart = false; // break out of slow start
				}
				unsigned long long int new_low = lastACK + 1;
				cw.setLowestSeqNum(new_low);
	    	}
	    }
	}
	cout << "final PKT = " << SEQ << endl;
	cout << "bytesRead = " << bytesRead << endl;
	cout << "Sending termination packet!\n";
	if ((numbytes = sendto(sockfd, END, 4, 0, p->ai_addr, p->ai_addrlen)) == -1) {
        perror("sender: sendto");
        exit(1);
    }
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {

	// set up UDP socket
	setup_UDP(hostname, hostUDPport);

	// initialize TCP connection
	if(initialize_TCP() == -1) {
		cout << "reliablyTransfer: Could not complete TCP handshake.\n";
		return;
	}

	CongestionWindow cw;
	// initialize the CongestionWindow
	cw.setLowestSeqNum(1);
	cw.setLastACK(0);
	cw.setWindowSize(1);
	cw.setHighestSeqNum(1);
	cw.setLastSent(0);

    sendPackets(filename, bytesToTransfer, cw);

    //freeaddrinfo(servinfo);
    close(sockfd);
    return; 
}
