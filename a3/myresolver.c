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
char* ROOT_IP[14] = {
		 "198.41.0.4"    , //a
		 "192.228.79.201", //b
		 "192.33.4.12"   , //c
		 "128.8.10.90"   , //d
		 "192.203.230.10", //e
		 "192.5.5.241"   , //f
		 "192.112.36.4"  , //g
		 "128.63.2.53"   , //h
		 "192.36.148.17" , //i
		 "192.58.128.30" , //j
		 "193.0.14.129"  , //k
		 "199.7.83.42"   , //l
		 "202.12.27.33"  , //m
};

struct LABEL_LIST* parseLabels(char* str) {
    struct LABEL_LIST* list = (struct LABEL_LIST*) malloc(
            sizeof(struct LABEL_LIST));
    int size = 0;
    uint8_t count = 0;
    char* itr = str;

    //Find size first
    while (1) {
        count = (uint8_t) *itr;
        itr++;
        // Reached end of string
        if (count == 0) {
            itr = str;
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
            cur->message = (char*) malloc(sizeof(char) * count);
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
void unpackExtendedMessageHeader(struct MESSAGE_HEADER* header,
        struct MESSAGE_HEADER_EXT* ret) {
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

    printf("Description: %u\n", header->description);
    ret->description.qr_type = header->description & DNS_QR_MASK;
    ret->description.opcode = header->description & DNS_OPCODE_MASK;
    ret->description.auth_answer = header->description & DNS_AA_MASK;
    ret->description.trunc = header->description & DNS_TRUNC_MASK;
    ret->description.recurse_desired = header->description & DNS_RD_MASK;
    ret->description.recurse_available = header->description & DNS_RA_MASK;
    ret->description.Z = header->description & DNS_Z_MASK;
    ret->description.resp_code = header->description & DNS_RCODE_MASK;
}

//ret assumed already allocated.
void repackExtendedMessageHeader(struct MESSAGE_HEADER_EXT* header,
        struct MESSAGE_HEADER* ret) {
    if (ret == NULL || header == NULL) {
        fprintf(stderr, "Uninitialized return struct.");
        return;
    }
    ret->id = header->id;
    ret->question_count = header->question_count;
    ret->answer_count = header->answer_count;
    ret->nameserver_count = header->nameserver_count;
    ret->additional_count = header->additional_count;

    ret->description = header->description.qr_type & header->description.opcode
            & header->description.auth_answer & header->description.trunc
            & header->description.recurse_desired
            & header->description.recurse_available & header->description.Z
            & header->description.resp_code;
}

int main(int argc, char *argv[]) {
    //Take the input string, pass to the parseLabel method.
    char name[20] = "www.google.com"; //For now, simply use a hard-coded domain.
    //Grabbed from A1 udp client
    char* root_name = ROOT_IP[2];
    int sockfd;
    struct sockaddr_in root_server;
    char port[6] = "53";

    // As per man page, set protocol to 0 (generic IP)
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		fprintf(stderr, "Failed to create socket.");
	}

    struct DNS_MESSAGE test_message;
    test_message.header.id = 10;
    test_message.header.additional_count = 0;
    test_message.header.answer_count = 0;
    test_message.header.description = 0;
    test_message.header.nameserver_count = 0;
    test_message.header.question_count = 0;

    struct MESSAGE_HEADER header;
    header.id = htons(10);
    header.additional_count = 0;
    header.answer_count = 0;
    header.description = 0;
    header.nameserver_count = 0;
    header.question_count = 0;

    fprintf(stderr, "Trying DNS server %s\n", root_name);

    root_server.sin_family = AF_INET;
    root_server.sin_len = 0;
    root_server.sin_port = htons(DNS_PORT);
    // Clear out the data.
    memset(root_server.sin_zero, '\0', sizeof(root_server.sin_zero));
    int result = inet_pton(AF_INET, root_name, &(root_server.sin_addr.s_addr));
    if (result != 1) {
    	if (result == 0) {
    		fprintf(stderr, "Invalid IP passed in.\n");
    	} else if (result < 0) {
    		fprintf(stderr, "Unable to parse IP address: %s\n", strerror(errno));
    	}
    	exit(1);
    }

    //SETUP a good message header for testing. 
    printf("Testing send\n");
    int size = sizeof(struct MESSAGE_HEADER);
    int bytesSent = 0;

    while (bytesSent < size) {
    	char ip[INET_ADDRSTRLEN];
    	inet_ntop(AF_INET, &(root_server), ip,INET_ADDRSTRLEN);
    	fprintf(stderr, "Sending %d bytes to %s\n", size, ip);
    	bytesSent = sendto(sockfd, (void*) (&header), size, 0, (struct sockaddr*) (&(root_server)), (socklen_t) (sizeof(struct sockaddr_in)));
    	fprintf(stderr, "Sent %d bytes\n", bytesSent);

    	if (bytesSent < 0) {
    		fprintf(stderr, "Failed to send. %s\n", strerror(errno));
    		exit(1);
    	}
    }

    fprintf(stderr, "TEST\n");
    printf("done sending... wait for reply!\n");
    void* data = malloc(sizeof(char) * MAX_UDP_SIZE);
    socklen_t recvLen;
    int bytesReceived = recvfrom(sockfd, data, MAX_UDP_SIZE, 0, (struct sockaddr*) (&(root_server.sin_addr)), &recvLen);
    printf("Received %d\n", bytesReceived);

    char* buf = data;
    struct MESSAGE_HEADER* respHeader = (struct MESSAGE_HEADER*) malloc(size);
    if (bytesReceived >= size) {
    	memcpy(respHeader, buf, size);
    	respHeader->id = ntohs(respHeader->id);
    	respHeader->description = ntohs(respHeader->description);
    	printf("Id: %u\n", respHeader->id);
    }
    fprintf(stderr, "TEST1\n");
    struct MESSAGE_HEADER_EXT* ret = malloc(sizeof(struct MESSAGE_HEADER_EXT));
    unpackExtendedMessageHeader(respHeader, ret);

    printf("Z = %d \n(Should be 0)\n", ret->description.Z);
    printf("respcode = %d \n(Should be error:1)\n", ret->description.resp_code);

    close(sockfd);
}
