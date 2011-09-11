#ifndef COMMON_H
#define COMMON_H
#include <stdlib.h>
#include "a1types.h"

packet_type parsePacketType(char* type);
char* parsePortNumber(int lower, int upper, char* value);
#endif
