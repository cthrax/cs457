#ifndef A1_TYPES_H
#define A1_TYPES_H

#define UDP 1
#define TCP 2
#define INVALID_PACKET_TYPE 3

typedef unsigned char version;
typedef unsigned int data;
typedef unsigned int packet_type;

struct reply_packet {
	version version;
}__attribute__((__packed__));

struct data_packet {
	version version;
	data data;

}__attribute__((__packed__));

#endif
