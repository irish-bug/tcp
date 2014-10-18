#include "sender.h"

/* SOCKET METHODS */

void Socket::setSockFD(int fd) {
	sockfd = fd;
}

int Socket::getSockFD() {
	return sockfd;
}

int Socket::setAddrInfo(struct addrinfo *info) {
	if (info == NULL) {
		cout << "Adress info struct is NULL.\n";
		return -1;
	}
    if (p== NULL) {
        cout << "Yo socket is null!!\n";
    }
	*p = *info;
	return 0;
}

struct addrinfo * Socket::getAddrInfo() {
	return p;
}

/* PACKET METHODS */

void Packet::setSequenceNum(unsigned long long int num) {
	sequence_num = num;
}

unsigned long long int Packet::getSequenceNum() {
	return sequence_num;
}

int Packet::setPacketData(char * buf, unsigned int size) {
	if (buf == NULL) {
		cout << "setPacketData: Packet data buffer is NULL.\n";
		return -1;
	}
	packet_len = size;
	strcpy(data, buf);
	return 0;
}

void Packet::getPacketData(char * buf) {
	strcpy(buf,data);
}

unsigned int Packet::getPacketSize() {
	return packet_len;
}

/* CONGESTION WINDOW METHODS */

/*CongestionWindow::CongestionWindow(int timeout) {
	lowest_seq_num = 1;
	last_ACK = 0;
	RTO = timeout;
	window_size = 1;
}*/

void CongestionWindow::setLowestSeqNum(unsigned long long int new_num) {
	lowest_seq_num = new_num;
}

unsigned long long int CongestionWindow::getLowestSeqNum() {
	return lowest_seq_num;
}
	
void CongestionWindow::setLastACK(unsigned long long int ACK_num) {
	last_ACK = ACK_num;
}

unsigned long long int CongestionWindow::getLastACK() {
	return last_ACK;
}
	
void CongestionWindow::setNewRTO(int timeout) {
	RTO = timeout;
}

void CongestionWindow::setWindowSize(int size) {
	window_size = size;
}

int CongestionWindow::getWindowSize() {
	return window_size;
}

void CongestionWindow::setDUPACKcounter(int val) {
	DUPACK_counter = val;
}

int CongestionWindow::getDUPACKcounter() {
	return DUPACK_counter;
}

void CongestionWindow::incrementDUPACKcounter() {
	DUPACK_counter += 1;
}

void CongestionWindow::sendWindow(int sockfd, struct addrinfo * p) {
	for (int i=0; i < window_size; i++) {
		int numbytes;
		char buf[MAX_DATA_SIZE];
		window[i].getPacketData(buf);
		unsigned int pkt_size = window[i].getPacketSize();
		if ((numbytes = sendto(sockfd, buf, pkt_size, 0,
	             p->ai_addr, p->ai_addrlen)) == -1) {
	        perror("sender: sendto");
	        exit(1);
	    }
	}
}

void CongestionWindow::addPacket(char * buf, unsigned int size, int sockfd, struct addrinfo * p) {
	int numbytes;
	Packet pkt;
    unsigned long long int seqnum;
    if(window.empty()) {
        seqnum = 1;
    }
    else {
        seqnum = window.back().getSequenceNum() + 1;
	}
    pkt.setSequenceNum(seqnum);
	pkt.setPacketData(buf, size);
	window.push_back(pkt); // make sure this will be FIFO
	string seq_num = to_string(seqnum);
	int seq_num_size = seq_num.size();
	int pkt_size = size + seq_num_size + 1; //data + sequence num + new line
	char msg[pkt_size];
	sprintf(msg,"%s\n%s", seq_num.c_str(), buf);
    cout << "Sending packet: " << msg << "\n";
	if ((numbytes = sendto(sockfd, msg, pkt_size, 0, p->ai_addr, p->ai_addrlen)) == -1) {
        perror("sender: sendto");
        exit(1);
    }
}

void CongestionWindow::removePackets(int n) {
	for(int i=0; i < n; i++) {
		window.pop_front();
	}
	lowest_seq_num = window.front().getSequenceNum();
}

int CongestionWindow::getNumPktsToAdd() {
	return window_size - window.size(); // number of packets that need to be added
}

void CongestionWindow::cutWindow(){
	window_size = window_size / 2;
}

void CongestionWindow::panicMode() {
	window_size = 1;
}


