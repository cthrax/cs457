#include <stdio.h>
#include <stdio.h>
#include "myresolver.h"

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

//IP Addresses : 202.12.27.33
//		 199.7.83.42
//		 193.0.14.129
//		 192.58.128.30
//		 192.36.148.17
//		 128.63.2.53
//		 Use port 53
char* ROOT_IP[6] = { "202.12.27.33",
   		    "199.7.83.42"  ,
  		    "193.0.14.129" ,
		    "192.58.128.30",
		    "192.36.148.17",
   		    "128.63.2.53"  ,
   		   };
struct LABEL_LIST* parseLabels(char* str) {
    struct LABEL_LIST* list = (struct LABEL_LIST*) malloc(sizeof(struct LABEL_LIST));
    int size = 0;
    uint8_t count = 0;
    char* itr = str;

    //Find size first
    while (1) {
        count = (uint8_t) *itr;
        itr++;
        // Reached end of string
        if (count == 0) {
            itr=str;
            break;
        } else {
            itr = itr + count;
            size++;
        }
    }

    list->labels = (struct LABEL*) malloc(sizeof(struct LABEL) * size);
    list->length = size;

    int labelCount = 0;
    int i = 0;
    while (1) {
        count = (uint8_t) *itr;
        itr++;
        //reached end of string
        if (count == 0) {
            break;
        } else {
            struct LABEL* cur = (struct LABEL*) malloc(sizeof(struct LABEL));
            cur->length = count;
            cur->message = (char*) malloc(sizeof(char)*count);
            for (; i < count; i++) {
                cur->message[i] = *itr;
                itr++;
            }

            list->labels[labelCount++] = *cur;
        }
    }

    return list;
}

//ret assumed already allocated.
void unpackExtendedMessageHeader(struct MESSAGE_HEADER* header, struct MESSAGE_HEADER_EXT* ret) {
    printf("Testing: unpack\n");
    if (ret == NULL || header == NULL) {
        fprintf(stderr, "Uninitialized return struct.");
        return;
    }
    ret->id = header->id;
    ret->question_count = header->question_count;
    ret->answer_count = header->answer_count;
    ret->nameserver_count = header->nameserver_count;
    ret->additional_count = header->additional_count;

    ret->description.qr_type =           header->description & DNS_QR_MASK;
    ret->description.opcode =            header->description & DNS_OPCODE_MASK;
    ret->description.auth_answer =       header->description & DNS_AA_MASK;
    ret->description.trunc =             header->description & DNS_TRUNC_MASK;
    ret->description.recurse_desired =   header->description & DNS_RD_MASK;
    ret->description.recurse_available = header->description & DNS_RA_MASK;
    ret->description.Z =                 header->description & DNS_Z_MASK;
    ret->description.resp_code =         header->description & DNS_RCODE_MASK;
}

//ret assumed already allocated.
void repackExtendedMessageHeader(struct MESSAGE_HEADER_EXT* header, struct MESSAGE_HEADER* ret) {
    if (ret == NULL || header == NULL) {
        fprintf(stderr, "Uninitialized return struct.");
        return;
    }
    ret->id = header->id;
    ret->question_count = header->question_count;
    ret->answer_count = header->answer_count;
    ret->nameserver_count = header->nameserver_count;
    ret->additional_count = header->additional_count;

    ret->description =
            header->description.qr_type &
            header->description.opcode &
            header->description.auth_answer &
            header->description.trunc &
            header->description.recurse_desired &
            header->description.recurse_available &
            header->description.Z &
            header->description.resp_code;
}

int main(int argc, char *argv[])
{
	//Take the input string, pass to the parseLabel method.
	char name[20] = "www.google.com";//For now, simply use a hard-coded domanin.
	struct hostent* host;
	struct DNS_MESSAGE test_message;
	
	//Grabbed from A1 udp client
	char* hostname = ROOT_IP[1];
	int sockfd;
   	struct addrinfo hints, *servinfo, *p;
  	int rv;
   	int numbytes;
   	int retval = 0;
   	void* buf;
   	char port[6] = "53";
   	
   	memset(&hints, 0, sizeof hints);
   	hints.ai_family = AF_UNSPEC;
  	hints.ai_socktype = SOCK_DGRAM;
  	
  	if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) 
  	{
        	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        	return 1;
    	}
    	
    	for(p = servinfo; p != NULL; p = p->ai_next) 
    	{
        	if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            		continue;
        	}

       		break;
    	}
    	
    	if (p == NULL) 
    	{
        	fprintf(stderr, "failed to bind socket\n");
        	return 2;
    	}
    	struct in_addr test;
    	test.s_addr  = inet_addr(hostname);
	host = gethostbyaddr( (const char*) &test, sizeof(struct in_addr), AF_INET );
	printf("%s\n",host->h_name);
	printf("%d\n",host->h_addrtype);
	printf("%d\n",host->h_length);
	if(host->h_aliases[0])
		printf("%s\n",host->h_aliases[0]);
	int i;
	for(i = 1; host->h_aliases[i]; i++)
        {
        	printf("%s\n",host->h_aliases[i]);
        }
        printf("Done with host aliases.\n");
        inet_ntoa(*( (struct in_addr *)host->h_addr) ) ;//convert to something readable
        if(host->h_addr_list[0])
        	printf("%s\n", host->h_addr_list[0]);
        for(i = 1; host->h_addr_list[i]; i++)
       {
       		inet_ntoa(*( (struct in_addr *)host->h_addr_list[i]) ) ;
       		printf("%s\n", host->h_addr_list[i]);
       	}
       	printf("Testing send\n");
       	if(sendto(sockfd, (void*)&test_message, 1, 0, p->ai_addr, p->ai_addrlen) < 0) //send DNS Message to the server
       	{
       		fprintf(stderr, "sendto error");
        	exit(1);
       	}
       	printf("done sending... wait for reply!");
       	void* data = malloc(sizeof(char) * MAX_UDP_SIZE);
       	recvfrom(sockfd, data, MAX_UDP_SIZE, 0, p->ai_addr, &(p->ai_addrlen));
       	printf("done recv from\n");
       	struct MESSAGE_HEADER_EXT* ret = malloc(sizeof(struct MESSAGE_HEADER_EXT));
       	unpackExtendedMessageHeader(&(((struct DNS_MESSAGE*)data)->header), ret);
       	printf("Z = %d \n(Should be 0)\n", ret->description.Z);
       	printf("respcode = %d \n(Should be error:1)\n", ret->description.resp_code);
       	
       	freeaddrinfo(servinfo);

    	close(sockfd);
}
