#include "sender.h"

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
	strncpy(data, buf, size);
	return 0;
}

void Packet::getPacketData(char * buf) {
	strcpy(buf,data);
}

unsigned int Packet::getPacketSize() {
	return packet_len;
}

/* CONGESTION WINDOW METHODS */

void CongestionWindow::setLowestSeqNum(unsigned long long int new_num) {
	lowest_seq_num = new_num;
}

unsigned long long int CongestionWindow::getLowestSeqNum() {
	return lowest_seq_num;
}
	
void CongestionWindow::setHighestSeqNum(unsigned long long int new_num) {
	highest_seq_num = new_num;
}

unsigned long long int CongestionWindow::getHighestSeqNum() {
	return highest_seq_num;
}

void CongestionWindow::setLastACK(unsigned long long int ACK_num) {
	last_ACK = ACK_num;
}

unsigned long long int CongestionWindow::getLastACK() {
	return last_ACK;
}

void CongestionWindow::setLastSent(unsigned long long int num) {
	lastSent = num;
}

unsigned long long int CongestionWindow::getLastSent() {
	return lastSent;
}

void CongestionWindow::setWindowSize(int size) {
	window_size = size;
}

int CongestionWindow::getWindowSize() {
	return window_size;
}

void CongestionWindow::addPacket(char * buf, unsigned int size, unsigned long long int seqnum, int sockfd, struct addrinfo * p) {
	int numbytes;
	Packet pkt;

    pkt.setSequenceNum(seqnum);
	pkt.setPacketData(buf, size);
	window.push_back(pkt); // make sure this will be FIFO

	/*string seq_num = to_string(seqnum);
	int seq_num_size = seq_num.size();
	int pkt_size = size + seq_num_size + 1; //data + sequence num + new line
	char msg[pkt_size];
	//char new_msg[pkt_size];

	char seq_num_str[seq_num_size];
	strcpy(seq_num_str, seq_num.c_str());
	strcat(seq_num_str,"\n");
	//sprintf(msg,"%s\n%s", seq_num.c_str(), buf);

	//strncpy(new_msg, msg, pkt_size);
    memcpy(msg, seq_num_str, seq_num_size + 1);
    memcpy(msg + seq_num_size + 1, buf, size);

    //cout << "Sending packet: " << msg << "\n";
    //cout << "PKT" << seqnum << " -->\n";
	if ((numbytes = sendto(sockfd, msg, pkt_size, 0, p->ai_addr, p->ai_addrlen)) == -1) {
        perror("sender: sendto");
        exit(1);
    }*/
}

unsigned long long int CongestionWindow::sendWindow(int sockfd, struct addrinfo * p) {
	int numbytes;
	unsigned long long int seqnum;

	for (int i=0; i<window_size; i++) {
	    seqnum = window[i].getSequenceNum();
		char data[MAX_DATA_SIZE];
		window[i].getPacketData(data);
		unsigned int size = window[i].getPacketSize();

		string seq_num = to_string(seqnum);
		int seq_num_size = seq_num.size();
		int pkt_size = size + seq_num_size + 1; //data + sequence num + new line
		char msg[pkt_size];
		//char new_msg[pkt_size];

		char seq_num_str[seq_num_size];
		strcpy(seq_num_str, seq_num.c_str());
		strcat(seq_num_str,"\n");

	    memcpy(msg, seq_num_str, seq_num_size + 1);
	    memcpy(msg + seq_num_size + 1, data, size);

	    //cout << "Sending packet: " << msg << "\n";
	    //cout << "PKT" << seqnum << " -->\n";
		if ((numbytes = sendto(sockfd, msg, pkt_size, 0, p->ai_addr, p->ai_addrlen)) == -1) {
	        perror("sender: sendto");
	        exit(1);
	    }	
	    lastSent = seqnum;
	    //cout << "lastSent =" << lastSent << endl;
	}
	return seqnum;
}

void CongestionWindow::sendPacket(int index, int sockfd, struct addrinfo * p) {
	Packet pkt = window[index];
	int numbytes;

	unsigned long long int seqnum = pkt.getSequenceNum();
	//cout << "PKT" << seqnum << endl;
	char data[MAX_DATA_SIZE];
	pkt.getPacketData(data);
	unsigned int size = pkt.getPacketSize();

	string seq_num = to_string(seqnum);
	int seq_num_size = seq_num.size();
	int pkt_size = size + seq_num_size + 1; //data + sequence num + new line
	char msg[pkt_size];

	char seq_num_str[seq_num_size];
	strcpy(seq_num_str, seq_num.c_str());
	strcat(seq_num_str,"\n");

	memcpy(msg, seq_num_str, seq_num_size + 1);
	memcpy(msg + seq_num_size + 1, data, size);

	if ((numbytes = sendto(sockfd, msg, pkt_size, 0, p->ai_addr, p->ai_addrlen)) == -1) {
		perror("sender: sendto");
		exit(1);
	}	
	//cout << "PKT" << seqnum << endl;
	//cout << "lastSent = " << lastSent << endl;
	lastSent = seqnum;
}

void CongestionWindow::removePackets(int n) {
	for(int i=0; i < n; i++) {
		window.pop_front();
	}
}

int CongestionWindow::getNumPktsToAdd() {
	if(window.empty()){
		return window_size;
	}
	else {
		//cout << "num_pkts = " << window_size - (int)window.size() << endl;
		return window_size - (int)window.size(); // number of packets that need to be added
	}
}

void CongestionWindow::cutWindow(){
	window_size = window_size / 2;
}

void CongestionWindow::panicMode() {
	window_size = 1;
}

