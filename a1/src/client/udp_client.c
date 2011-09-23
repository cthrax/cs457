#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <inttypes.h>

#include "udp_client.h"
#include "a1types.h"

const int BUF_SIZE = 5;

int start_udp_client(char* hostname, char* port, unsigned int msg)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct reply_packet *reply;
    struct data_packet data;
    int retval = 0;
    void* buf;
    buf = malloc(sizeof(struct reply_packet));

    if (buf == NULL) {
    	fprintf(stderr, "Failed to allocate memory for buffer.\n");
    	return 1;
    }


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "failed to bind socket\n");
        return 2;
    }

    data.version = (version)1;
    data.data = htonl(msg);
    if ((numbytes = sendto(sockfd, (void*)&data, sizeof(data), 0, p->ai_addr, p->ai_addrlen)) == -1) {
        fprintf(stderr, "sendto error");
        exit(1);
    } else {
    	printf("Sent number (%u) and flag(%u) to server %s:%s via UDP.\n", data.data, data.version, hostname, port);
    }

    if (numbytes != sizeof(data)) {
    	fprintf(stderr, "couldn't send all of message.\n");
    	retval = 3;
    }

    //Setup for client timeout
	struct timeval  t;
	t.tv_sec  = 3;
	t.tv_usec = 0;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t)) < 0) {
		fprintf(stderr, "Error setting timeout condition.\n");
		return 8;
	}

	if ((numbytes = recvfrom(sockfd, buf, BUF_SIZE, 0, p->ai_addr, &(p->ai_addrlen))) == -1) {
		fprintf(stderr, "Server didn't respond in a reasonable time or there was an error with the socket.\n");
		retval = 4;
	} else {
		reply = (struct reply_packet*)buf;
		int recevedData = reply->version;
		if(recevedData == 1) {
			printf("SUCCESS\n");
		} else {
			fprintf(stderr, "Invalid response received from server.\n");
		}
	}

    freeaddrinfo(servinfo);

    close(sockfd);

    return retval;
}
