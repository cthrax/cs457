#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "udp_server.h"

// IP's lower than 41951 are reserved according to http://en.wikipedia.org/wiki/List_of_TCP_and_UDP_port_numbers
const int LOWER_IP = 41952;
const int UPPER_IP = 65535;

const int UDP = 1;
const int TCP = 2;

void toLower(char* target);

int main(int argc, char **argv) {
	char* port = NULL;
	int index;
	int c;
	int test = 0;
	int type = 0;
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
				test = atoi(optarg);
				if (test >= LOWER_IP && test <= UPPER_IP) {
					port = optarg;
				} else {
					fprintf(stderr, "Invalid port %d. Must use ephemeral or dynamic port within range of 49152-65535 so as to preven stepping on registered ports.\n", test);
				}
				break;
			case 't':
				//Value ucp or tcp
				toLower(optarg);

				if (strcmp(optarg, "udp") == 0) {
					type = UDP;
				} else if (strcmp(optarg, "tcp") == 0) {
					type = TCP;
				} else {
					fprintf(stderr, "Invalid type, must be tcp or udp.");
				}

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
	}

	return retstatus;
}

void toLower(char* target) {
	int i = 0;
	for (i = 0; i < 3; i++) {
		if (target[i] == '\n') {
			break;
		}
		if (isupper(target[i])) {
			target[i] = tolower(target[i]);
		}
	}
}
