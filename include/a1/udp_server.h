#include <netinet/in.h>

void* get_in_addr(struct sockaddr *sa);
int start_udp_server(char* port);
