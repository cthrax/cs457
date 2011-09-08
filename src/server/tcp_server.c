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

#include "tcp_server.h"

int start_tcp_server(char* port) {

	int serversockfd; // server structure
	struct addrinfo serverside, *serverinfo, *p;
	int rv;
	char str[INET6_ADDRSTRLEN];
	struct sockaddr * ai_addr;

	struct sockaddr_storage new_addr; // new socket from 'accept() call' details stored in new_addr
	int newsockfd;
	socklen_t new_size;

	int num_bytes;
	char buff[5];

	memset(&serverside,0,sizeof serverside);
	serverside.ai_family = AF_INET6;
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

	printf("\n\n");
	printf("Socket Descriptor successful ...\n\n");

	if (p == NULL) {

		fprintf(stderr, "Failed to bind Socket...\n\n");
		exit(1);
	}
	printf("Bind Successful...\n\n");

	freeaddrinfo(serverinfo);

	if (listen(serversockfd, 10) == -1) {

		fprintf(stderr, "Listening Error..\n\n");
		exit(1);
	}

	inet_ntop(AF_INET6, &(((struct sockaddr_in6 *) (&(ai_addr)))->sin6_addr), str, INET6_ADDRSTRLEN);

	printf("Server waiting/listening on port:  %s and IP: %s\n\n", port, str);

	while (1) {

		new_size = sizeof new_addr;
		newsockfd = accept(serversockfd, (struct sockaddr *) &new_addr,
				&new_size);

		if (newsockfd == -1) {

			fprintf(stderr, "Accept Error..\n\n");
			continue;

		}

		inet_ntop(new_addr.ss_family,
				&(((struct sockaddr_in6 *) (&(new_addr)))->sin6_addr), str,
				INET6_ADDRSTRLEN);

		printf("Server got connection from %s\n\n", str);

		if ((num_bytes = recv(newsockfd, buff, sizeof buff, 0)) == -1) {

			fprintf(stderr, "Recv Error..\n\n");
			printf("num bytes recieved: %d\n", num_bytes);
		}

		printf("Server recieved %s , i.e. %d bytes from IP: %s\n\n", buff,
				num_bytes, str);

	}

	close(newsockfd);

	return 0;

}
