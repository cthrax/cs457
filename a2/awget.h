#ifndef AWGET_H_
#define AWGET_H_
/* Holds the structures used by both AWGET & SS*/
#include <stdlib.h>

// Struct to hold Version, No. of Hops and URL Length
struct ss_packet
{
	uint8_t version;
	uint8_t step_count;
	char* url;
	struct int_tuple* steps;
}__attribute__((__packed__));

// <<IP,Port>> Pair Sturcture
struct int_tuple
{
	uint32_t ip_addr;
	uint16_t port_no;
}__attribute__((__packed__));

//Structure to hold IP in dotted quad notation
struct char_ip
{
	char ch_ip[17];
};
#endif
