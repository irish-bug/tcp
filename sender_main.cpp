#include <stdio.h>
#include <stdlib.h>
#include <ctime>
#include "sender.h"

#define ACK "ACK"
#define SYN "SYN"
#define ACK_SIZE 128
#define THRESHOLD 64 // TCP uses a threshold of 64KB

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
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
	unsigned long long int bytesRead;
	// check for legit file
	ifstream myFile(filename);
	if(!myFile.is_open()) {
		cout << "reliablyTransfer: Unable to open file.\n"
		return;
	}
	// set up UDP connection
	Socket * sock;
	if((sock = setup_UDP(hostname,hostUDPport)) == NULL) {
		cout << "reliablyTransfer: Socket object is NULL.\n"
	}

	// initialize TCP connection
	int timeout;
	if((timeout = initialize_TCP(sock)) == -1) {
		cout << "reliablyTransfer: Could not complete TCP handshake.\n"
		return;
	}
	
	CongestionWindow cw;
	cw.setLowestSeqNum(1);
	cw.setLastACK(0);
	cw.setNewRTO(timeout);
	cw.setWindowSize(1);
	cw.setDUPACKcounter(0);

	// THIS LOOP DOES SLOW START WITH MSS = 1
	bool slowStart = TRUE;
	while ((bytesRead < bytesToTransfer) && slowStart) {
		
		// read file into packets and place in cw vector
		int num_pkts, bytes;

		if((num_pkts = cw.getNumPktsToAdd()) < 0) {
			num_pkts = 0;
		}

		char buf[MAX_DATA_SIZE];
		for(int i=0; i<num_pkts; i++) {
			if((bytes = myFile.read(buf, MAX_DATA_SIZE)) != -1) { // read 1KB into buf
				bytesRead += bytes;
			}
			cw.addPacket(buf);
		}

		// send the congestion window
		cw.sendWindow(sock);

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
				removePackets(diff); // pop off ACKd packets
				// increase congestion window
				int ws = cw.getWindowSize() + 1;
				cw.setWindowSize(ws);
				if(ws >= THRESHOLD) {
					slowStart = FALSE; // break out of slow start
				}
			}
		}
		else {
			//timed out. reset CW to 1
			cw.panicMode();
		}

	}
}

unsigned long long int getACKnum(char * msg) {
	// strip the ACK number from message
	string ACKmsg(msg);

}

int initialize_TCP(Socket * sock) {
	char buf[MAXBUFLEN]; //store 

	clock_t start, stop;
	if(sock == NULL) {
		cout << "initialize_TCP: Socket object is NULL.\n"
		return -1;
	}

	start = clock();
	if ((numbytes = sendto(sock->sockfd, SYN, strlen(SYN), 0,
             sock->p->ai_addr, sock->p->ai_addrlen)) == -1) {
        perror("sender: sendto");
        exit(1);
    }
    
    if ((numbytes = recvfrom(sockfd, buf, MAX_DATA_SIZE, 0,
        	sock->p->ai_addr, sock->p->ai_addrlen)) == -1) {
        perror("recvfrom");
        exit(1);
    }
    stop = clock();

    string received (buf);

    if (received.compare("ACK") != 0) {
    	cout << "Received: " << received << ".\n";
    	return -1;
    }
    else {
    	cout << "ACK received!\n"
    }

    if ((numbytes = sendto(sock->sockfd, ACK, strlen(ACK), 0,
             sock->p->ai_addr, sock->p->ai_addrlen)) == -1) {
        perror("sender: sendto");
        exit(1);
    }

	return (stop - start / (double) CLOCKS_PER_SEC); // return estimated timeout
}

Socket * setup_UDP(char * hostname, unsigned short int port) {
	int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
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
        return 2;
    }

    Socket * sock;
    sock->setSockFD(sockfd);
    sock->setAddrInfo(p);

    /*if ((numbytes = sendto(sockfd, argv[2], strlen(argv[2]), 0,
             p->ai_addr, p->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }

    freeaddrinfo(servinfo);

    printf("talker: sent %d bytes to %s\n", numbytes, argv[1]);
    close(sockfd);*/

    return sock;
}
