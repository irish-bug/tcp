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

unsigned long long int SEQ; // keeps track of SEQ for adding packets

int sockfd;
struct addrinfo * p;
CongestionWindow cw;
unsigned long long int bytes;
char * file;

thread receiver;
thread sender;
mutex ACK_lock;
unsigned long long int lastACK;
int timeout_FLAG;
int done_FLAG;
unsigned int DUPACKctr;
unsigned long long int bytesSent;

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
	int running = 1;
	while (running) {
		timeout = 0;
		if ((numbytes = recvfrom(sockfd, buf, MAX_DATA_SIZE, 0,
	        	p->ai_addr, (socklen_t *)&p->ai_addrlen)) == -1) {
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

	    	if (memcmp(buf,END,4) == 0) {
	    		return;
	    	}
	    	unsigned long long int ACKnum = getACKnum(buf);
	    	// check if DUPACK

	    	if (ACKnum == lastACK) {
	    		ACK_lock.lock();
	    		DUPACKctr++;
	    		ACK_lock.unlock();
	    		bytesSent = ACKnum * 1024;
	    	}
	    	else if (ACKnum > lastACK) {
	    		ACK_lock.lock();
	    		lastACK = ACKnum;
	    		//cout << "lastACK = " << lastACK << endl;
	    		//cout << "lastSent = " << cw.getLastSent() << endl;
	    		DUPACKctr = 0;
	    		if(done_FLAG && (lastACK == cw.getLastSent())) {
	    			running = 0;
	    		}
	    		bytesSent = ACKnum * 1024;
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

	while (1) {
		//cout << "bytes = " << bytes << endl;
		//cout << "bytesRead = " << bytesRead << endl;
		ACK_lock.lock();
		if(bytesSent >= bytes) {
			ACK_lock.unlock();
			break;
		}
		ACK_lock.unlock();
		int numPktsToAdd, numPktsToSend, index, packet_size;

		numPktsToAdd = cw.getNumPktsToAdd();

		if(numPktsToAdd < 0) {
			numPktsToAdd = 0;
		}

		char buf[MAX_DATA_SIZE];
		for(int i=0; i<numPktsToAdd; i++) {
			//if (bytesRead >= bytes) { break; }

			unsigned long long int diff = bytes - bytesRead;
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

		

		// read file into packets and place in cw vector

		// CRITICAL SECTION
		ACK_lock.lock(); // to read.
		if (timeout_FLAG) {
			cw.panicMode();
			slowStart = true;
            timeout_FLAG = 0;
		}
		else {
			//timed out. reset CW to 1

			if(lastACK == cw.getLastACK()) {
				//cout << "DUPACK!\n";
				if(DUPACKctr >= 5) { // we're getting DUPACKs
					//cout << "window_size = " << cw.getWindowSize() << endl;
					cw.cutWindow();
					unsigned long long int newSEQ = cw.sendWindow(sockfd, p); // resend the window!
					//cout << "resending window!\n";
					SEQ = newSEQ + 1;
					DUPACKctr = 0;
				}
				//cw.setHighestSeqNum(cw.getLowestSeqNum + cw.getWindowSize() - 1);
			}
			else if (lastACK > cw.getLastACK()) {
				int ws;
				//cout << "lastACK = " << lastACK << endl;
				cw.setLastACK(lastACK);
				int diff = lastACK - cw.getLowestSeqNum() + 1;
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
				unsigned long long int new_low = lastACK + 1;
				//unsigned long long int new_high = ws + new_low - 1;
				cw.setLowestSeqNum(new_low);
				//cw.setHighestSeqNum(new_high);
			}
			else {
				// got an earlier ACK - we don't care
			}
		}
		ACK_lock.unlock();
		// END CRITICAL SECTION
	}
	cout << "final PKT = " << SEQ << endl;
	cout << "bytesRead = " << bytesRead << endl;
	cout << "Sending termination packet!\n";
	int numbytes;
	if ((numbytes = sendto(sockfd, END, 4, 0, p->ai_addr, p->ai_addrlen)) == -1) {
        perror("sender: sendto");
        exit(1);
    }
    ACK_lock.lock();
    done_FLAG = 1;
    ACK_lock.unlock();
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
	cw.setLowestSeqNum(1);
	cw.setLastACK(0);
	cw.setWindowSize(1);
	cw.setHighestSeqNum(1);
	cw.setLastSent(0);

	// initialize the globals
	lastACK = 0;
	DUPACKctr = 0;
    timeout_FLAG = 0;
    done_FLAG = 0; // set to one when all pkts sent
    SEQ = 1;
    bytesSent = 0;

    // create the sender thread
	sender = thread(sendPackets);
	// create the receiver thread
	receiver = thread(receiveACKs);

	sender.join();
    receiver.join();

    //freeaddrinfo(servinfo);
    close(sockfd);
    return; 
}
