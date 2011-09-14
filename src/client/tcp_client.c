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

#include "a1types.h"

int start_tcp_client(char* hostname, char* port, unsigned int data) {

	int sockfd;
	char s[INET6_ADDRSTRLEN];
	struct addrinfo hints, *servinfo, *p;
	int rv, num_bytes;
	char ssbuff[10]="";
	
	struct reply_packet *reply;
   	struct data_packet dataP;
   	
   	void* buf;
  	buf = malloc(sizeof(struct reply_packet));
   	
  	//Endianess for our purposes only cares about byteorder, not bit order, so one byte is safe.
   	dataP.version = (version)1;
   	dataP.data = htonl(data);//conver to network byte order (big Endien)

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        	return 1;
	}
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) 
		{
			fprintf(stderr, "Socket Error..\n\n");//Do we need this error message? or should we just do 1 big one at the end?
			continue;
		}
		break;
	} // end of for loop

	if ((rv = connect(sockfd, p->ai_addr, p->ai_addrlen)) < 0) {//several less-than-zero error codes...
	    //should we add a time-out condition? otherwise this'll sit here forever until a server accepts.
		//no, connect is non-blocking and either succeeds or fails, according to man page.
		close(sockfd);
		fprintf(stderr, "Connect Error (Is server there?): %s\n\n", gai_strerror(rv));
		return 2;//continue;
	}

	if (p == NULL) {
		fprintf(stderr,"Failed to Connect..\n\n");
		exit(1);
	}
	if ((inet_ntop(p->ai_family, &(((struct sockaddr_in *) (p->ai_addr))->sin_addr), s, sizeof s)) == NULL) {
		printf("Error: inet_ntop failed\n");
		return 7;
	}
	printf("Client connected to IP: %s on port #: %s.\n\n", hostname, port);
	
	//Copy + Send the structure
	memcpy(ssbuff, &dataP.version, sizeof(dataP.version));
	memcpy(ssbuff+sizeof(dataP.version), &dataP.data, sizeof(dataP.data));
	//
	if((num_bytes = send(sockfd, &ssbuff, (sizeof(dataP)+1), 0)) < 0)
	{
		printf("Error: Send failed\n");
	}
	else
	{
		printf("Sent %u to IP %s on port %s using tcp.\n", ntohl(dataP.data), hostname, port);
	}

	freeaddrinfo(servinfo);
    //Setup for client timeout
	struct timeval  t;
	t.tv_sec  = 3;
	t.tv_usec = 0;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t)) < 0) {
		fprintf(stderr, "Error setting timeout condition.\n");
		return 8;
	}

	if ((num_bytes = recv(sockfd, buf, sizeof(struct reply_packet), 0)) == -1) {
		fprintf(stderr, "Server didn't respond in a reasonable time or there was an error with the socket.\n");
		return 9;
	}

	reply = (struct reply_packet*)buf;
	if(reply->version == 1) {
		printf("Success.\n");
	} else {
		printf("Invalid response from server: %u\n", reply->version);
	}

	close(sockfd);
	free(buf);
	return 0;
}
