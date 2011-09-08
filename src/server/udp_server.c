#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "udp_server.h"
#define MAXBUFLEN 100

// get sockaddr, IPv4 or IPv6:
void* get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int start_udp_server(char* port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    int retstatus = 0;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "failed to get addr info: %s\n", gai_strerror(rv));
        retstatus = 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL && retstatus == 0; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            fprintf(stderr, "failed to create socket.\n");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            fprintf(stderr, "failed to bind to address.\n");
            continue;
        }

        break;
    }

    if (p == NULL && retstatus == 0) {
        fprintf(stderr, "listener: failed to bind socket\n");
        retstatus = 2;
    }

    freeaddrinfo(servinfo);

    while (1 && retstatus == 0) {
		addr_len = sizeof their_addr;
		if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
			fprintf(stderr, "recvfrom failed.");
			retstatus = 1;
			break;
		}

		buf[numbytes] = '\0';
		retstatus = 0;
    }

    close(sockfd);
    return retstatus;
}
