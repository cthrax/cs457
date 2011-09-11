#include <ctype.h>
#include <string.h>
#include "common.h"
#include "a1types.h"

void _toLower(char* target);

packet_type parsePacketType(char* type) {
	_toLower(type);

	if (strcmp(type, "udp") == 0) {
		return UDP;
	} else if (strcmp(type, "tcp") == 0) {
		return TCP;
	} else {
		return INVALID_PACKET_TYPE;
	}
}

char* parsePortNumber(int lower, int upper, char* value) {
	int test = atoi(value);
	if (test >= lower && test <= upper) {
		return value;
	} else {
		return NULL;
	}
}

void _toLower(char* target) {
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
