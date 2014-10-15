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
using namespace std;

#define MAX_DATA_SIZE 1024
#define SYN "SYN"
#define ACK "ACK"

int sockfd;
struct addrinfo hints, *servinfo, *p;
struct sockaddr_storage their_addr;
socklen_t addr_len;
unsigned long long int last_SEQ;

void reliablyReceive(unsigned short int myUDPport, char* destinationFile);

int main(int argc, char** argv)
{
	unsigned short int udpPort;
	
	if(argc != 3)
	{
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		exit(1);
	}
	
	udpPort = (unsigned short int)atoi(argv[1]);
	
	reliablyReceive(udpPort, argv[2]);
}

int setup_UDP(unsigned short int port) {
    int rv;
    int numbytes;
    char buf[MAXBUFLEN];
    char s[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP


    string port_str = to_string(port);
    if ((rv = getaddrinfo(NULL, port_str.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    printf("listener: waiting to recvfrom...\n");

    addr_len = sizeof their_addr;
    /*if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }

    printf("listener: got packet from %s\n",
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s));
    printf("listener: packet is %d bytes long\n", numbytes);
    buf[numbytes] = '\0';
    printf("listener: packet contains \"%s\"\n", buf);

    close(sockfd);*/

    return 0;
}

int initialize_TCP() {
	int numbytes;
	char buf[MAX_DATA_SIZE];

	if ((numbytes = recvfrom(sockfd, buf, MAX_DATA_SIZE, 0,
        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }

    if (strcmp(buf,SYN) != 0) {
    	cout << "Received: " + buf + "\n";
    	cout << "Not a SYN... terminating.\n";
    	return -1; // indicates termination
    }

	if ((numbytes = sendto(sockfd, ACK, strlen(ACK), 0,
         (struct sockaddr *)&their_addr, &addr_len)) == -1) {
    	perror("sender: sendto");
        exit(1);
    }
    return 0;
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {

	//try to open file
	ofstream myFile(destinationFile);
	if (!myFile.is_open()) {
		cout << "reliablyReceive: Unable to open file.\n";
		return;
	}

	if (setup_UDP(myUDPport) != 0) {
		cout << "reliablyReceive: UDP socket setup failed... terminating.\n";
		return -1;
	}

	if (initialize_TCP() == -1) {
		cout << "reliablyReceive: TCP connection could not be initialized... terminating.\n";
		return -1;
	}

	last_SEQ = 0;
	bool running = true; // set to false when END packet received
	while (running) {
		if ((numbytes = recvfrom(sockfd, buf, MAX_DATA_SIZE, 0,
	        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
	        perror("recvfrom");
	        exit(1);
	    }

	    if (strcmp(buf,END) == 0) {
	    	cout << "Got a termination packet!\n";
	    	running = false;
	    }

	    unsigned long long int SEQ_num;
	    sscanf(buf, "%llu", &SEQ_num);

	    char ACK_msg[MAX_DATA_SIZE];
	    if (SEQ_num > (last_SEQ + 1)) { 
	    	// we're missing a packet! resend the last ACK.
	    	sprintf(ACK_msg, "ACK %llu", last_SEQ);
	    	if ((numbytes = sendto(sockfd, ACK_msg, strlen(ACK_msg), 0,
         		(struct sockaddr *)&their_addr, &addr_len)) == -1) {
    			perror("sender: sendto");
        		exit(1);
    		}	
	    }
	    else if (SEQ_num == (last_SEQ + 1)) {
	    	// in order packet! write it to file.
	    	char * msg;
	    	msg = strchr(buf,'\n'); 
	    	myFile.write(msg);
	    	// send an ACK!
	    	sprintf(ACK_msg, "ACK %llu", last_SEQ+1);
	    	if ((numbytes = sendto(sockfd, ACK_msg, strlen(ACK_msg), 0,
         		(struct sockaddr *)&their_addr, &addr_len)) == -1) {
    			perror("sender: sendto");
        		exit(1);
    		}
    		last_SEQ += 1; // change the last_SEQ number
	    }
	}

	return 0;
}