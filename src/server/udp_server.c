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

#include <netinet/in.h>
#include "udp_server.h"
#include "server_common.h"

int start_udp_server(char* port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    void *buf;
    socklen_t addr_len;
    int retstatus = 0;
    struct data_packet *data;
    struct reply_packet reply;

    buf = malloc(sizeof(struct data_packet));

    if (buf == NULL) {
    	fprintf(stderr, "Unable to allocate buffer for request.");
    	return 1;
    }

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
    printf("Server waiting/listening on port:  %s \n\n", port);
    while (1 && retstatus == 0) {
		addr_len = sizeof their_addr;
		if ((numbytes = recvfrom(sockfd, buf, sizeof(struct data_packet), 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
			fprintf(stderr, "recvfrom failed.");
			retstatus = 1;
			break;
		}

		data = (struct data_packet*)buf;
		data->data = ntohl(data->data);

		if (data->version != 1) {
			fprintf(stderr, "Invalid packet received %u\n", data->version);
			printf("the number is: %u\n", data->data);
			continue;
		} else {
			printf("The number is: %u\n", data->data);
			fflush(stdout);
		}

		//Endianess for our purposes only cares about byteorder, not bit order, so one byte is safe.
		reply.version = (version)1;
		if ((numbytes = sendto(sockfd, (void*)&reply, sizeof(reply), 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
			fprintf(stderr, "Unable to send reply.");
		}

		retstatus = 0;
    }

    close(sockfd);
    return retstatus;
}
