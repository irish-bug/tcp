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
	*p = *info;
	return 0;
}

struct addrinfo * Socket::getAddrInfo() {
	return p;
}

/* PACKET METHODS */

Packet::Packet(int num, char * buf) {
	sequence_num = num;
	if (buf != NULL) {
		strcpy(data,buf);
	}
}

void Packet::setSequenceNum(unsigned long long int num) {
	sequence_num = num;
}

int Packet::setPacketData(char * buf) {
	if (buf == NULL) {
		cout << "setPacketData: Packet data buffer is NULL.\n";
		return -1;
	}
	strcpy(data,buf);
	return 0;
}

unsigned long long int Packet::getSequenceNum() {
	return sequence_num;
}

/* CONGESTION WINDOW METHODS */

CongestionWindow::CongestionWindow(int timeout) {
	lowest_seq_num = 1;
	last_ACK = 0;
	RTO = timeout;
	window_size = 1;
}

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

void CongestionWindow::sendWindow(Socket * sock) {
	for (int i=0; i < window_size; i++) {
		if ((numbytes = sendto(sock->sockfd, window[i], strlen(window[i]), 0,
	             sock->p->ai_addr, sock->p->ai_addrlen)) == -1) {
	        perror("sender: sendto");
	        exit(1);
	    }
	}
}

void CongestionWindow::addPacket(char * buf) {
	Packet pkt;
	pkt.setSequenceNum(window.back().getSequenceNum() + 1);
	pkt.setPacketData(buf);
	window.push_back(pkt); // make sure this will be FIFO
}

void CongestionWindow::removePackets(int n) {
	for(int i=0; i < n; i++) {
		window.pop_front();
	}
	lowest_seq_num = window.front().sequence_num;
}

int CongestionWindow::getNumPktsToAdd() {
	return window_size - window.size(); // number of packets that need to be added
}
void CongestionWindow::panicMode() {
	window_size = 1;
}


