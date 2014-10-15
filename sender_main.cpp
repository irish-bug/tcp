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
#include "sender.h"
using namespace std;

#define ACK "ACK"
#define SYN "SYN"
#define ACK_SIZE 128
#define THRESHOLD 64 // TCP uses a threshold of 64KB

int sockfd;
struct addrinfo * p;

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
	string ACKmsg(msg);
	return 0;
}

int initialize_TCP() {
	char buf[4]; //store 
	buf[3] = '\0';

	struct timeval tim;

	gettimeofday(&tim, NULL);
	double start = tim.tv_sec+(tim.tv_usec/1000000.0);
	int numbytes;

	cout << "SYN -->\n";
	if ((numbytes = sendto(sockfd, SYN, 3, 0,
             p->ai_addr, p->ai_addrlen)) == -1) {
        perror("sender: sendto");
        exit(1);
    }
    
    // TODO: check for timeout here
    if ((numbytes = recvfrom(sockfd, buf, 3, 0,
        	p->ai_addr, (socklen_t *)&p->ai_addrlen)) == -1) {
        perror("recvfrom");
        exit(1);
    }
    gettimeofday(&tim, NULL);
    double stop = tim.tv_sec+(tim.tv_usec/1000000.0);

    string received (buf);

    if (received.compare("ACK") != 0) {
    	cout << "Received: " << received << ".\n";
    	return -1;
    }
    else {
    	cout << "ACK <--\n";
    }

	return (stop - start); // return estimated timeout
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

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
	unsigned long long int bytesRead;
	// check for legit file
	ifstream myFile(filename);
	if(!myFile.is_open()) {
		cout << "reliablyTransfer: Unable to open file.\n";
		return;
	}
	// set up UDP connection
	setup_UDP(hostname, hostUDPport);

	// initialize TCP connection
	int timeout;
	if((timeout = initialize_TCP()) == -1) {
		cout << "reliablyTransfer: Could not complete TCP handshake.\n";
		return;
	}
	
	CongestionWindow cw;
	cw.setLowestSeqNum(1);
	cw.setLastACK(0);
	cw.setNewRTO(timeout);
	cw.setWindowSize(1);
	cw.setDUPACKcounter(0);

	// THIS LOOP DOES SLOW START WITH MSS = 1
	bool slowStart = true;
	while ((bytesRead < bytesToTransfer) && slowStart) {
		
		// read file into packets and place in cw vector
		int num_pkts, bytes;

		if((num_pkts = cw.getNumPktsToAdd()) < 0) {
			num_pkts = 0;
		}

		char buf[MAX_DATA_SIZE];
		for(int i=0; i<num_pkts; i++) {
			/*if((bytes = myFile.read(buf, MAX_DATA_SIZE)) != -1) { // read 1KB into buf
				bytesRead += bytes;
			}*/
            myFile.read(buf, MAX_DATA_SIZE);
            bytesRead += myFile.gcount();
			cw.addPacket(buf);
		}

		// send the congestion window
		cw.sendWindow(sockfd, p);

		// wait for an ACK until timeout
		char resp[ACK_SIZE];

		if (not timeout) {
			unsigned long long int ACK_num = getACKnum(resp);
			if (ACK_num == cw.getLastACK()) {
				cw.incrementDUPACKcounter();
				if(cw.getDUPACKcounter() == 3) {
					//3 DUPACKS, cut the window!
				}
			}
			else {
				cw.setLastACK(ACK_num);
				int diff = ACK_num - cw.getLowestSeqNum();
				cw.removePackets(diff); // pop off ACKd packets
				// increase congestion window
				int ws = cw.getWindowSize() + 1;
				cw.setWindowSize(ws);
				if(ws >= THRESHOLD) {
					slowStart = false; // break out of slow start
				}
			}
		}
		else {
			//timed out. reset CW to 1
			cw.panicMode();
		}
	}
}
