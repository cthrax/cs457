// Server
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<netdb.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<arpa/inet.h>

#include <netinet/in.h>//POSIX hton/ntoh
#include "tcp_server.h"
#include "server_common.h"

int start_tcp_server(char* port) {

	int serversockfd; // server structure
	struct addrinfo serverside, *serverinfo, *p;
	int rv;
	char str[INET_ADDRSTRLEN];

	struct data_packet *dataP;
	struct reply_packet response;
	
  	//Endianess for our purposes only cares about byteorder, not bit order, so one byte is safe.
	response.version = 1;

	struct sockaddr_storage new_addr; // new socket from 'accept() call' details stored in new_addr
	int newsockfd;
	socklen_t new_size;

	int num_bytes;
	void* buf;
	
	buf = malloc(sizeof(struct data_packet));

	memset(&serverside,0,sizeof serverside);
	serverside.ai_family = AF_INET;
	serverside.ai_socktype = SOCK_STREAM;
	serverside.ai_flags = AI_PASSIVE;
	serverside.ai_protocol = 0;

	if ((rv = getaddrinfo(NULL, port, &serverside, &serverinfo)) != 0) {

		fprintf(stderr, "Getaddrinfo error..\n\n");
		exit(1);
	}

	for (p = serverinfo; p != NULL; p = p->ai_next) {

		if ((serversockfd
				= socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			fprintf(stderr, "Socket Error..\n\n");
			continue;
		}

		if (bind(serversockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(serversockfd);
			fprintf(stderr, "Bind Error..\n\n");
			continue;

		}

		break;
	} // for loop ends

	if (p == NULL) {

		fprintf(stderr, "Failed to bind Socket...\n\n");
		exit(1);
	}

	freeaddrinfo(serverinfo);

	if (listen(serversockfd, 10) == -1) {

		fprintf(stderr, "Listening Error..\n\n");
		exit(1);
	}

	inet_ntop(AF_INET, p->ai_addr, str, p->ai_addrlen);

	printf("Server waiting/listening on port:  %s and IP: %s\n\n", port, str);

	while (1) {

		new_size = sizeof new_addr;
		newsockfd = accept(serversockfd, (struct sockaddr *) &new_addr, &new_size);

		if (newsockfd == -1) {
			fprintf(stderr, "Accept Error..\n\n");
			continue;
		}

		inet_ntop(new_addr.ss_family, &(((struct sockaddr_in *) (&(new_addr)))->sin_addr), str, INET_ADDRSTRLEN);

		printf("Server got connection from %s\n\n", str);

		if ((num_bytes = recv(newsockfd, buf, sizeof (struct data_packet), 0)) == -1) {

			fprintf(stderr, "Recv Error..\n\n");
			printf("num bytes recieved: %d\n", num_bytes);
		}

		dataP = (struct data_packet*) buf;
		dataP->data = ntohl(dataP->data);
		printf("Server recieved %u with flag %u , i.e. %d bytes from IP: %s\n\n", dataP->data, dataP->version,
				num_bytes, str);
		//send responce:
		if(dataP->version == 1) {
			if((num_bytes = send(newsockfd, (void*)&response, sizeof(response), 0)) < 0) {
				printf("Error sending response!\n");
				return 2;
			}
		}
		else {
			printf("Error: received bad packet!\n");
		}

	}

	free(buf);
	close(newsockfd);

	return 0;

}
