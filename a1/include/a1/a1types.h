#include <netinet/in.h>

#ifndef A1_TYPES_H
#define A1_TYPES_H

#define UDP 1
#define TCP 2
#define INVALID_PACKET_TYPE 3

typedef uint8_t version;
typedef uint32_t data;
//Changed to the uintxx_t for use of hton(s/l) which is required for the project
typedef unsigned int packet_type;//We shouldn't have this in any of the packets as it could break compatibility with other groups server.

struct reply_packet {
	version version;
}__attribute__((__packed__));

struct data_packet {
	version version;
	data data;

}__attribute__((__packed__));

#endif
