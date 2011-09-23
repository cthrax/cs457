#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "udp_server.h"
#include "tcp_server.h"
#include "common.h"

// IP's lower than 41952 are reserved according to http://en.wikipedia.org/wiki/List_of_TCP_and_UDP_port_numbers
const int LOWER_IP = 41952;
const int UPPER_IP = 65535;

int main(int argc, char **argv) {
	char* port = NULL;
	int index;
	int c;
	packet_type type = INVALID_PACKET_TYPE;
	int retstatus = -1;

	// Turn off default error handling for getopt, return '?' instead
	opterr = 0;

	if (argc == 1) {
		fprintf(stderr, "Must specify port and packet type.\n");
		return 1;
	}

	while ((c = getopt(argc, argv, "p:t:")) != -1) {
		switch (c) {
			case 'p':
				if (parsePortNumber(LOWER_IP, UPPER_IP, optarg) == NULL) {
					fprintf(stderr, "Invalid port %s. Must use ephemeral or dynamic port within range of 49152-65535 so as to preven stepping on registered ports.\n", optarg);
					return 1;
				} else {
					port = optarg;
				}
				break;
			case 't':
				//Value ucp or tcp
				type = parsePacketType(optarg);
				break;
			case '?':
				if (optopt == 't') {
					fprintf(stderr, "Option -%c requires an argument. (tcp | udp)\n", optopt);
				} else if (optopt == 'p') {
					fprintf(stderr, "Option -%c requires an argument (-p <port number from 49152-65535>)\n", optopt);
				} else if (isprint(optopt)) {
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				} else {
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
				}
				return 1;
			default:
				abort();
		}
	}

	for (index = optind; index < argc; index++) {
		printf("Non-option argument %s, ignoring.\n", argv[index]);
	}

	if (type == UDP) {
		retstatus = start_udp_server(port);
	} else if (type == TCP) {
		retstatus = start_tcp_server(port);
	} else {
		retstatus = 2;
		fprintf(stderr, "Invalid type, must be tcp or udp.");
	}

	return retstatus;
}
