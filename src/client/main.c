#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// The client should not be restricted in port range because the server could be on any port it wants (technically).
const int LOWER_IP = 1;
const int UPPER_IP = 65535;
void toLower(char* target);

int main(int argc, char **argv) {
	char* port = NULL;
	int index;
	int c;
	int test = 0;

	opterr = 0;

	if (argc == 1) {
		fprintf(stderr, "Must specify port and packet type.\n");
		return 1;
	}

	while ((c = getopt(argc, argv, "p:t:x:h:")) != -1) {
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
					printf("udp!\n");
				} else if (strcmp(optarg, "tcp") == 0) {
					printf("tcp!\n");
				} else {
					fprintf(stderr, "Invalid type, must be tcp or udp.");
				}

				break;
			case 'x':
				// Data to send
				break;
			case 'h':
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
	return 0;
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