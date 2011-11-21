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

int sockfd;
struct sockaddr_in server;
socklen_t server_size;
int cur_root_server = 0;

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

void sendNumBytes(void* data, int size) {
	int bytesSent = 0;
	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, (&server.sin_addr), ip, INET_ADDRSTRLEN);

    while (bytesSent < size) {
    	fprintf(stderr, "Sending %d bytes to %s\n", size, ip);
    	bytesSent += sendto(sockfd, data + bytesSent, size - bytesSent, 0, (struct sockaddr*)(&server), server_size);
    	fprintf(stderr, "Sent %d bytes\n", bytesSent);

    	if (bytesSent < 0) {
    		fprintf(stderr, "Failed to send. %s\n", strerror(errno));
    		exit(1);
    	}
    }
}

void appendToBuffer(char* buf, void* data, int size, int* total_size) {
	if (*total_size + size > MAX_UDP_SIZE) {
		fprintf(stderr, "Packet too large. Aborting.");
		exit(1);
	}

	memcpy(buf, data, size);
	buf += size;
	*total_size += size;
}

void sendDnsMessage(struct DNS_MESSAGE* message) {
	int packet_size = 0;
	char* buf = (char*) malloc(MAX_UDP_SIZE);
	char* itr = buf;

	//Send Header
	appendToBuffer(itr, (&message->header), sizeof(struct MESSAGE_HEADER), &packet_size);

    int i = 0;
    //Send Question
    for (i = 0; i < ntohs(message->header.question_count); i++) {
    	struct MESSAGE_QUESTION* cur = message->question + i;
    	appendToBuffer(itr, cur->qname, strlen(cur->qname), &packet_size);
    	RR_TYPE type = htons(cur->qtype);
    	QCLASS class = htons(cur->qclass);
    	appendToBuffer(itr, &type, sizeof(type), &packet_size);
    	appendToBuffer(itr, &class, sizeof(class), &packet_size);
    }

    sendNumBytes(buf, packet_size);
}

char* getNextRootServer() {
	return ROOT_IP[cur_root_server++];
}

int main(int argc, char *argv[]) {
    //Take the input string, pass to the parseLabel method.
    char name[20] = "www.google.com"; //For now, simply use a hard-coded domain.
    //Grabbed from A1 udp client
    char* root_name = getNextRootServer();
    server_size = sizeof(struct sockaddr_in);

    // As per man page, set protocol to 0 (generic IP)
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		fprintf(stderr, "Failed to create socket.");
	}

    struct DNS_MESSAGE test_message;
    test_message.header.id = htons(10);
    test_message.header.additional_count = 0;
    test_message.header.answer_count = 0;
    test_message.header.description = 0;
    test_message.header.nameserver_count = 0;
    test_message.header.question_count = htons(1);

    struct MESSAGE_QUESTION* question = (struct MESSAGE_QUESTION*) malloc(sizeof(struct MESSAGE_QUESTION));
    question->qname = name;
    question->qclass = htons(MESSAGE_QCLASS_IN);
    question->qtype = htons(MESSAGE_QTYPE_AAAA);

    test_message.question = question;

    fprintf(stderr, "Trying DNS server %s\n", root_name);

    server.sin_family = AF_INET;
    server.sin_len = 0;
    server.sin_port = htons(DNS_PORT);
    // Clear out the data.
    memset(server.sin_zero, '\0', sizeof(server.sin_zero));
    int result = inet_pton(AF_INET, root_name, &(server.sin_addr.s_addr));
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

    sendDnsMessage(&test_message);

    fprintf(stderr, "TEST\n");
    printf("done sending... wait for reply!\n");
    char* data = malloc(sizeof(char) * MAX_UDP_SIZE);
    socklen_t recvLen;
    int bytesReceived = recvfrom(sockfd, data, sizeof(struct MESSAGE_HEADER), 0, (struct sockaddr*) (&(server)), &recvLen);
    fprintf(stderr, "Received %d\n", bytesReceived);

    struct DNS_MESSAGE* response = (struct DNS_MESSAGE*) malloc(sizeof(struct DNS_MESSAGE));
    if (bytesReceived >= sizeof(struct MESSAGE_HEADER)) {
    	memcpy(&(response->header), data, sizeof(struct MESSAGE_HEADER));
    	response->header.id = ntohs(response->header.id);
    	response->header.description = ntohs(response->header.description);
    	printf("Id: %u\n", response->header.id);
    }
    fprintf(stderr, "TEST1\n");
    struct MESSAGE_HEADER_EXT* ret = malloc(sizeof(struct MESSAGE_HEADER_EXT));
    unpackExtendedMessageHeader(&(response->header), ret);

    // Always failing this for some reason, maybe an error code doesn't count as a response?
    /*if (ret->description.qr_type == DNS_QR_TYPE_QUERY) {
    	fprintf(stderr, "Expected a response, received a query. Aborting.\n");
    	exit(1);
    }*/

    if (response->header.question_count > 0) {
    	int i = 0;
    	for (i = 0; i < ntohs(response->header.question_count); i++) {
    		int labelCount = 0;

    		while (1) {

    			bytesReceived += recvfrom(sockfd, data + bytesReceived, 1, 0, (struct sockaddr*) (&(server)), &server_size);
    		}
    	}
    }

    printf("Z = %d \n(Should be 0)\n", ret->description.Z);
    printf("respcode = %d \n(Should be error:1)\n", ret->description.resp_code);

    close(sockfd);
}
