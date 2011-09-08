#ifndef SERVER_COMMON_H
#define SERVER_COMMON_H
#include <netinet/in.h>

void* get_in_addr(struct sockaddr *sa);
#endif
