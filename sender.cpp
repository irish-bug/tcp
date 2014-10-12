#include "sender.h"

Packet::Packet(int num, char * buf) {
	sequence_num = num;
	if (buf != NULL) {
		strcpy(data,buf);
	}
}

CongestionWindow::CongestionWindow(int timeout) {
	lowest_seq_num = 1;
	last_ACK = 0;
	RTO = timout;
	window_size = 1;
	vector<Packet> window; // alloc Packet structs as data is read
}