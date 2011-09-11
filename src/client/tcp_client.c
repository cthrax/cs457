// Client


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

int start_tcp_client(char* hostname, char* port, unsigned int data) {

	//TODO: Do we need the newfd?
	int sockfd, newfd;
	char s[INET6_ADDRSTRLEN];
	struct addrinfo hints, *servinfo, *p;
	int rv, num_bytes;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	char temp;

	//p=&hints;


	if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "Getaddrinfo Error.. check hostname.\n\n");
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {

		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
				== -1) {

			fprintf(stderr, "Socket Error..\n\n");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			fprintf(stderr, "Connect Error..\n\n");
			continue;

		}

		break;
	} // end of for loop


	if (p == NULL) {
		fprintf(stderr,"Failed to Connect..\n\n");
		exit(1);
	}

	inet_ntop(p->ai_family, &(((struct sockaddr_in6 *) (p->ai_addr))->sin6_addr), s, sizeof s);

	printf("Client connected to IP: %s\n\n", s);

	freeaddrinfo(servinfo);

//TODO: Get the actual data structure to send.
	/*if (num_bytes = send(sockfd, argv[1], sizeof argv[1], 0) == -1) {

		perror("Send Error..\n\n");

		close(sockfd);
	}*/
	printf("%d ", temp);
	printf("Num Bytes Sent: %d\n", num_bytes);
	close(sockfd);

	return 0;
}
