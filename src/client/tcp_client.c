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

	//TODO: Do we need the newfd?
	int sockfd, newfd;
	char s[INET6_ADDRSTRLEN];
	struct addrinfo hints, *servinfo, *p;
	int rv, num_bytes;
	char ssbuff[10]="";
	
	struct reply_packet *reply;
   	struct data_packet dataP;
   	
   	void* buf;
  	buf = malloc(sizeof(struct reply_packet));
   	
   	dataP.version = htons(1);
   	dataP.data = htonl(data);//conver to network byte order (big Endien)

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	//hints.ai_protocol = 0;
	char temp;

	//p=&hints;


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
	if (rv = connect(sockfd, p->ai_addr, p->ai_addrlen) < 0) {//several less-than-zero error codes...
	//should we add a time-out condition? otherwise this'll sit here forever until a server accepts. 
		close(sockfd);
		fprintf(stderr, "Connect Error:%s\n\n", gai_strerror(rv));
		return 2;//continue;
	}

	if (p == NULL) {
		fprintf(stderr,"Failed to Connect..\n\n");
		exit(1);
	}
	
	char * serverip = (char*) malloc(sizeof(char));
	if(serverip == NULL)
	{
		printf("Error: Malloc-serverip\n");
		return 6;//return
	}
	serverip = inet_ntop(p->ai_family, &(((struct sockaddr_in6 *) (p->ai_addr))->sin6_addr), s, sizeof s);
	if(serverip == NULL)
	{
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
	//Setup wait time:
	struct timeval  timer;
	int rcvtimeo=3000;
	timer.tv_sec  =  rcvtimeo / 1000;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timer, sizeof(timer)) < 0)
	{
		printf("Error: Timeout before getting a responce\n");
		return 8;
	}
	if ((num_bytes = recv(sockfd, buf, sizeof(struct reply_packet), 0)) == -1)
	{
		printf("Error: Recieve failed to get the packet\n");
		return 9;
	}
	reply = (struct reply_packet*)buf;
	temp = ntohs(reply->version);
	if(temp == 1)
		printf("Success.\n");
	else
		printf("Invalid responce from server: %u\n", ntohs(reply->version));
	close(sockfd);
	//free(serverip);
	//free(buf);
	return 0;
}
