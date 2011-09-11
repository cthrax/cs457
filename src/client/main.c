#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

#include "common.h"
#include "tcp_client.h"
#include "udp_client.h"
#include "a1types.h"

// The client should not be restricted in port range because the server could be on any port it wants (technically).
const int LOWER_IP = 1;
const int UPPER_IP = 65535;

int main(int argc, char **argv) {
	char* port = NULL;
	char* host = NULL;
	int index;
	int c;
	int test;
	unsigned int msg = 0;
	packet_type type = INVALID_PACKET_TYPE;


	opterr = 0;

	if (argc == 1) {
		fprintf(stderr, "Must specify port, packet type, server, and data\n");
		return 1;
	}

	while ((c = getopt(argc, argv, "p:t:x:s:")) != -1) {
		switch (c) {
			case 'p':
				if (parsePortNumber(LOWER_IP, UPPER_IP, optarg) == NULL) {
					fprintf(stderr, "Invalid port %s. Must use ephemeral or dynamic port within range of 49152-65535 so as to preven stepping on registered ports.\n", optarg);
				} else {
					port = optarg;
				}
				break;
			case 't':
				//Value ucp or tcp
				type = parsePacketType(optarg);
				break;
			case 'x':
				// Data to send
				test = atoi(optarg);

				if ((*optarg != '0' && test == 0) ||
				    //(test == INT_MAX || test == INT_MIN) || Need to find these consts
				    (test < 0)) {

					fprintf(stderr, "Invalid data. Data must be a positive 32 bit integer.");
					return 1;
				}
				msg = (unsigned int)test;
				break;
			case 's':
				if (gethostbyname(optarg) == NULL) {
					fprintf(stderr, "Invalid hostname.\n");
					return 2;
				}
				host = optarg;
				break;
			case '?':
				if (optopt == 't') {
					fprintf(stderr, "Option -%c requires an argument. (tcp | udp)\n", optopt);
				} else if (optopt == 'p') {
					fprintf(stderr, "Option -%c requires an argument (-p <port number from 1-65535>)\n", optopt);
				} else if (optopt == 's') {
					fprintf(stderr, "Option -%c requires an argument (-s <valid hostname>)\n", optopt);
				} else if (optopt == 'x') {
					fprintf(stderr, "Option -%c requires an argument (-x <some positive 32 bit int>)\n", optopt);
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
		start_udp_client(host, port, msg);
	} else if (type == TCP) {
		start_tcp_client(host, port, msg);
	} else {
		fprintf(stderr, "Invalid packet type, must be tcp or udp.");
	}
	return 0;
}
