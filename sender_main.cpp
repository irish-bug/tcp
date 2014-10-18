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
#define ACK_SIZE 128
#define THRESHOLD 64 // TCP uses a threshold of 64KB
#define TIMEOUT 30 // in milliseconds

int sockfd;
struct addrinfo * p;
CongestionWindow cw;
unsigned long long int bytes;
//ifstream myFile;
char * file;

thread receiver;
thread sender;
mutex ACK_lock;
unsigned long long int lastACK;
int timeout_FLAG;
unsigned int DUPACKctr;

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
	char buf[4]; //store 
	buf[3] = '\0';

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

    //string received (buf);
    cout << buf << "\n";

    if (strcmp(buf,"ACK") != 0) {
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
    int numbytes;

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

    /*if ((numbytes = sendto(sockfd, argv[2], strlen(argv[2]), 0,
             p->ai_addr, p->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }

    freeaddrinfo(servinfo);

    printf("talker: sent %d bytes to %s\n", numbytes, argv[1]);
    close(sockfd);*/
}

void receiveACKs() {
	// listen for ACKs
	// set a timeout on the recvfrom syscall
	int numbytes, timeout;
	char buf[MAX_DATA_SIZE];

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 30000;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
	    perror("Error");
	}

	while (1) {
		timeout = 0;
		if ((numbytes = recvfrom(sockfd, buf, MAX_DATA_SIZE, 0,
	        	p->ai_addr, (socklen_t *)&p->ai_addrlen)) == -1) {
	        //perror("recvfrom");
	    	if(errno == EAGAIN || errno == EWOULDBLOCK) {
	    		timeout = 1;
	    	}
	    	else {
	        	exit(1);
	        }
	    }

	    if(timeout) {
	    	ACK_lock.lock();
	    	timeout_FLAG = 1;
	    	ACK_lock.unlock();
	    }
	    else {
	    	// we got an ACK
	    	unsigned long long int ACKnum = getACKnum(buf);
	    	// check if DUPACK
	    	if (ACKnum == lastACK) {
	    		ACK_lock.lock();
	    		DUPACKctr++;
	    		ACK_lock.unlock();
	    	}
	    	else if (ACKnum > lastACK) {
	    		ACK_lock.lock();
	    		lastACK = ACKnum;
	    		ACK_lock.unlock();
	    	}
	    }
	}

}

void sendPackets() {
	unsigned long long int bytesRead = 0;
	bool slowStart = true;
	int partialPacket = 0;

	ifstream myFile(file);
	if(!myFile.is_open()) {
		cout << "reliablyTransfer: Unable to open file.\n";
		return;
	}
	while (bytesRead < bytes) {
		
		// read file into packets and place in cw vector
		int num_pkts, bytes;

		if((num_pkts = cw.getNumPktsToAdd()) < 0) {
			num_pkts = 0;
		}

		char buf[MAX_DATA_SIZE];
		for(int i=0; i<num_pkts; i++) {
            myFile.read(buf, MAX_DATA_SIZE);
            cout << buf << "\n";
            bytesRead += myFile.gcount();
			cw.addPacket(buf, myFile.gcount(), sockfd, p); // this adds packets and sends them!
		}

		// CRITICAL SECTION
		ACK_lock.lock(); // to read.
		if (timeout_FLAG) {
			cw.panicMode();
            timeout_FLAG = 0;
		}
		else {
			//timed out. reset CW to 1
			if(lastACK == cw.getLastACK()) {
				if(DUPACKctr >= 3) { // we're getting DUPACKs
					cw.cutWindow();
				}
			}
			else if (lastACK > cw.getLastACK()) {
				int ws;
				cw.setLastACK(lastACK);
				int diff = lastACK - cw.getLowestSeqNum();
				cw.removePackets(diff); // pop off ACKd packets
				// increase congestion window
				if(slowStart){ws = cw.getWindowSize() + 1;}
				else{
					partialPacket++;
					if(partialPacket == cw.getWindowSize()) {ws = cw.getWindowSize() + 1;}
				}
				cw.setWindowSize(ws);
				if(ws >= THRESHOLD) {
					slowStart = false; // break out of slow start
				}
			}
			else {
				// got an earlier ACK - we don't care
			}
		}
		ACK_lock.unlock();
		// END CRITICAL SECTION
	}
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
	bytes = bytesToTransfer;

	file = filename;

	// set up UDP socket
	setup_UDP(hostname, hostUDPport);

	// initialize TCP connection
	if(initialize_TCP() == -1) {
		cout << "reliablyTransfer: Could not complete TCP handshake.\n";
		return;
	}
	
	// initialize the CongestionWindow
	//CongestionWindow cw;
	cw.setLowestSeqNum(1);
	cw.setLastACK(0);
	cw.setWindowSize(1);

	// initialize the globals
	lastACK = 0;
	DUPACKctr = 0;
    timeout_FLAG = 0;

    // create the sender thread
	sender = thread(sendPackets);
	// create the receiver thread
	receiver = thread(receiveACKs);

	sender.join();
    receiver.join();
}
