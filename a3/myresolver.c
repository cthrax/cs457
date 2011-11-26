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
    ret->id = ntohs(header->id);
    ret->question_count = ntohs(header->question_count);
    ret->answer_count = ntohs(header->answer_count);
    ret->nameserver_count = ntohs(header->nameserver_count);
    ret->additional_count = ntohs(header->additional_count);

    printf("Description: %u\n", header->description);
    uint16_t desc = ntohs(header->description);
    ret->description.qr_type = desc & DNS_QR_MASK;
    ret->description.opcode = desc & DNS_OPCODE_MASK;
    ret->description.auth_answer = desc & DNS_AA_MASK;
    ret->description.trunc = desc & DNS_TRUNC_MASK;
    ret->description.recurse_desired = desc & DNS_RD_MASK;
    ret->description.recurse_available = desc & DNS_RA_MASK;
    ret->description.Z = desc & DNS_Z_MASK;
    ret->description.resp_code = desc & DNS_RCODE_MASK;
}

//ret assumed already allocated.
void repackExtendedMessageHeader(struct MESSAGE_HEADER_EXT* header,
        struct MESSAGE_HEADER* ret) {
    if (ret == NULL || header == NULL) {
        fprintf(stderr, "Uninitialized return struct.");
        return;
    }
    ret->id = htons(header->id);
    ret->question_count = htons(header->question_count);
    ret->answer_count = htons(header->answer_count);
    ret->nameserver_count = htons(header->nameserver_count);
    ret->additional_count = htons(header->additional_count);

    ret->description = htons(header->description.qr_type & header->description.opcode
            & header->description.auth_answer & header->description.trunc
            & header->description.recurse_desired
            & header->description.recurse_available & header->description.Z
            & header->description.resp_code);
}

void sendNumBytes(void* data, int size, struct sockaddr_in* server) {
	int bytesSent = 0;
	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, (&(server->sin_addr)), ip, INET_ADDRSTRLEN);

    while (bytesSent < size) {
    	fprintf(stderr, "Sending %d bytes to %s\n", size, ip);
    	bytesSent += sendto(sockfd, data + bytesSent, size - bytesSent, 0, (struct sockaddr*)(server), server_size);
    	fprintf(stderr, "Sent %d bytes\n", bytesSent);

    	if (bytesSent < 0) {
    		fprintf(stderr, "Failed to send. %s\n", strerror(errno));
    		exit(1);
    	}
    }
}

int recvNumBytes(char* buf, int size, struct sockaddr_in* server) {
	int bytesReceived = 0;
	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, (&(server->sin_addr)), ip, INET_ADDRSTRLEN);

    while (bytesReceived < size) {
    	fprintf(stderr, "Receiving %d bytes from %s\n", size - bytesReceived, ip);
    	bytesReceived += recvfrom(sockfd, buf + bytesReceived, size - bytesReceived, 0, (struct sockaddr*)(server), &server_size);
    	fprintf(stderr, "Received %d bytes\n", bytesReceived);

    	if (bytesReceived < 0) {
    		fprintf(stderr, "Failed to receive. %s\n", strerror(errno));
    		//TODO: should this actually exit?
    		exit(1);
    	}
    }

    return bytesReceived;
}

void appendToBuffer(char** buf, void* data, int size, int* total_size) {
	if (*total_size + size > MAX_UDP_SIZE) {
		fprintf(stderr, "Packet too large. Aborting.");
		exit(1);
	}

	fprintf(stderr, "Adding to buffer %d bytes\n", size);
	memcpy(*buf, data, size);
	(*buf) += size;
	*total_size += size;
}

int labelLen(uint8_t* label) {
	int len = 0;
	uint8_t* itr = label;
	while (1) {
		uint8_t size = *itr;
		if (size == 0) {
			// Add the last zero
			len += 1;
			break;
		}

		len += size + 1;
		itr += size + 1;
	}

	return len;
}

void sendDnsMessage(struct DNS_MESSAGE* message, struct sockaddr_in* server) {
	int packet_size = 0;
	char* buf = (char*) malloc(MAX_UDP_SIZE);
	char* itr = buf;
	printf("packet size: %d\n", packet_size);

	//Send Header
	appendToBuffer(&itr, (&(message->header)), sizeof(struct MESSAGE_HEADER), &packet_size);

    int i = 0;
    //Send Question
    printf("Found %d questions.\n", ntohs(message->header.question_count));
    for (i = 0; i < ntohs(message->header.question_count); i++) {
    	struct MESSAGE_QUESTION* cur = message->question + i;
    	// Append name
    	appendToBuffer(&itr, cur->qname, labelLen(cur->qname), &packet_size);
    	// Append type
    	printf("Appending qtype: %02x\n", ntohs(cur->qtype));
    	appendToBuffer(&itr, &(cur->qtype), sizeof(cur->qtype), &packet_size);
    	// Append class
    	printf("Appending qclass: %02x\n", ntohs(cur->qclass));
    	appendToBuffer(&itr, &(cur->qclass), sizeof(cur->qclass), &packet_size);
    }

    printf("finished packet size: %d\n", packet_size);
    sendNumBytes(buf, packet_size, server);
}

char* getNextRootServer() {
	return ROOT_IP[cur_root_server++];
}

// str must be at least 256 char long as names are a maximum of 255 octets (including label length)
void createCharFromLabel(uint8_t* label, char* str) {
    uint8_t* itr = label;
    int i = 0;
    while(1) {
        uint8_t size = (uint8_t) *itr;

        if (size == 0) {
            str[i] = '\0';
            break;
        }

        itr++;
        int curI = i;
        for (;i < curI + size; i++) {
            str[i] = (char) *itr;
            itr++;
        }
        str[i++] = '.';
    }
}

// A label can be a max of 63 octets.
// Str is expected to be a full host name, meaning the last char should be '.'
void createLabelFromChar(char* str, uint8_t** label) {
    char cur = '\0';
    uint8_t size = 0;
    int strLen = strlen(str);
    uint8_t counter = 0;

    // Create memory for the labels
    // Breakdown:
    // each '.' is replaced with a string length octet in the label
    // the '\0' is replaced with the 0 length size
    (*label) = (uint8_t*) malloc(sizeof(uint8_t) * strLen);

    uint8_t* itr = (*label);

    //Initialize first label length.
    (*itr) = 0;
    itr++;
    cur = *(str + counter);
    while (1) {
        if (cur == '\0') {
            // Go back and set size
            (*(itr-size - 1)) = size;
            // Add zero length for label termination
            *(itr++) = 0;
            break;
        }

        if (cur == '.') {
            // Assign size now that it's known
            (*(itr - size - 1)) = size;
            // Move to next character position
            itr++;
            // Reset size counter
            size = 0;
            counter++;
            cur = *(str + counter);
            continue;

        }

        size++;
        counter++;
        //Assign char to position.
        (*itr) = (uint8_t) cur;

        //Increment pointer to next position
        itr++;

        //Get next char in str.
        cur = *(str + counter);
    }
}

RR_TYPE getRrType(char* buf) {
	RR_TYPE type;
	memcpy(&type, buf, sizeof(RR_TYPE));
	return ntohs(type);
}

QCLASS getClassType(char* buf) {
	QCLASS class;
	memcpy(&class, buf, sizeof(QCLASS));
	return ntohs(class);
}

uint32_t getUint32(char* buf) {
	uint32_t ret;
	memcpy(&ret, buf, sizeof(uint32_t));
	return ntohl(ret);
}

uint16_t getUint16(char* buf) {
	uint16_t ret;
	memcpy(&ret, buf, sizeof(uint16_t));
	return ntohs(ret);
}

void getQuestion(struct MESSAGE_QUESTION** question, int count, int *bytesParsed, char* buf) {
    if (count > 0) {
    	*question = (struct MESSAGE_QUESTION*)malloc(sizeof(struct MESSAGE_QUESTION) * count);
    	int i = 0;
    	for (i = 0; i < count; i++) {
    		struct MESSAGE_QUESTION *newMsg = (struct MESSAGE_QUESTION*) malloc(sizeof(struct MESSAGE_QUESTION));
    		char* start = buf + *bytesParsed;
    		int startPos = *bytesParsed;
    		while (1) {
    			//TODO: Handle compression
    			//Get size of label
    			uint8_t curSize = (uint8_t) *(buf + *bytesParsed);
    			*bytesParsed += 1;
    			if (curSize == 0) {
    				break;
    			}
    			*bytesParsed += curSize;
    		}

    		// Populate label
    		int labelSize = *bytesParsed - startPos;
    		newMsg->qname = (uint8_t *)malloc(labelSize);
    		memcpy(newMsg->qname, start, labelSize);

    		// Populate type
    		newMsg->qtype = getRrType(buf + *bytesParsed);
    		*bytesParsed += sizeof(RR_TYPE);

    		// Populate class
    		newMsg->qclass = getClassType(buf + *bytesParsed);
    		*bytesParsed += sizeof(QCLASS);

    		// Add to parent structure
    		*((*question) + i) = *newMsg;
    	}
    }
}

void getResourceRecord(struct MESSAGE_RESOURCE_RECORD** record, int count, int *bytesParsed, char* buf) {
    if (count > 0) {
    	*record = (struct MESSAGE_RESOURCE_RECORD*) malloc(sizeof(struct MESSAGE_RESOURCE_RECORD)* count);
    	int i = 0;
    	for (i = 0; i < count; i++) {
    		struct MESSAGE_RESOURCE_RECORD *newRecord = (struct MESSAGE_RESOURCE_RECORD*) malloc(sizeof(struct MESSAGE_RESOURCE_RECORD));
    		char* start = buf + *bytesParsed;
    		int startPos = *bytesParsed;
    		while (1) {
    			//TODO: Handle compression
    			//Get size of label
    			uint8_t curSize = (uint8_t) *(buf + *bytesParsed);
    			*bytesParsed += 1;
    			if (curSize == 0) {
    				break;
    			}

    			// Get chars of label
    			*bytesParsed += curSize;
    		}

    		// Populate name
    		int labelSize = *bytesParsed - startPos;
    		newRecord->name = (uint8_t *)malloc(labelSize);
    		memcpy(newRecord->name, start, labelSize);

    		// Populate type
    		newRecord->type = getRrType(buf + *bytesParsed);
    		*bytesParsed += sizeof(RR_TYPE);

    		// Populate class
    		newRecord->class = getClassType(buf + *bytesParsed);
    		*bytesParsed += sizeof(QCLASS);

    		// Populate TTL
    		newRecord->ttl = getUint32(buf + *bytesParsed);
    		*bytesParsed += sizeof(uint32_t);

    		// Populate RD Length
    		newRecord->rdlength = getUint16(buf + *bytesParsed);
    		*bytesParsed += sizeof(uint16_t);

    		// Populate RD Data
    		newRecord->rd_data = malloc(newRecord->rdlength);
    		memcpy(newRecord->rd_data, buf + *bytesParsed, newRecord->rdlength);
    		*bytesParsed += newRecord->rdlength;

    		*((*record) + i) = *newRecord;
    	}
    }
}

void queryForNameAt(char* name, char* root_name) {

    struct DNS_MESSAGE test_message;
    test_message.header.id = htons(10);
    test_message.header.additional_count = htons(0);
    test_message.header.answer_count = htons(0);
    test_message.header.description = htons(0);
    test_message.header.nameserver_count = htons(0);
    test_message.header.question_count = htons(1);

    struct MESSAGE_QUESTION* question = (struct MESSAGE_QUESTION*) malloc(sizeof(struct MESSAGE_QUESTION));
    uint8_t* name_label;
    createLabelFromChar(name, &name_label);
    question->qname = name_label;
    question->qclass = htons(MESSAGE_QCLASS_IN);
    question->qtype = htons(MESSAGE_QTYPE_AAAA);

    test_message.question = question;

    fprintf(stderr, "Trying DNS server %s\n", root_name);

    // Setup server to be queried
    struct sockaddr_in server;
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
    printf("Sending message.\n");
    sendDnsMessage(&test_message, &server);
    printf("done sending... wait for reply!\n");

    char* data = malloc(sizeof(char) * MAX_UDP_SIZE);
    socklen_t recvLen;
    int bytesReceived = recvfrom(sockfd, data, MAX_UDP_SIZE, 0, (struct sockaddr*) (&(server)), &recvLen);
    int bytesParsed = 0;
    //int bytesReceived = recvNumBytes(data,sizeof(struct MESSAGE_HEADER), &server);

    struct DNS_MESSAGE* response = (struct DNS_MESSAGE*) malloc(sizeof(struct DNS_MESSAGE));
    if (bytesReceived >= sizeof(struct MESSAGE_HEADER)) {
    	memcpy(&(response->header), data, sizeof(struct MESSAGE_HEADER));
    	response->header.id = ntohs(response->header.id);
    	response->header.description = ntohs(response->header.description);
    	printf("Id: %u\n", response->header.id);
    	bytesParsed += sizeof(struct MESSAGE_HEADER);
    }
    struct MESSAGE_HEADER_EXT* ret = malloc(sizeof(struct MESSAGE_HEADER_EXT));
    unpackExtendedMessageHeader(&(response->header), ret);

    // Always failing this for some reason, maybe an error code doesn't count as a response?
    if (ret->description.qr_type == DNS_QR_TYPE_QUERY) {
    	fprintf(stderr, "Expected a response, received a query. Aborting.\n");
    	exit(1);
    }

    fprintf(stderr, "Fetching response question.\n");
    getQuestion(&response->question, ret->question_count, &bytesParsed, data);
    fprintf(stderr, "Fetching response answer.\n");
    getResourceRecord(&response->answer, ret->answer_count, &bytesParsed, data);
    fprintf(stderr, "Fetching response authority.\n");
    getResourceRecord(&response->authority, ret->nameserver_count, &bytesParsed, data);
    fprintf(stderr, "Fetching response additional.\n");
    getResourceRecord(&response->additional, ret->additional_count, &bytesParsed, data);

    printf("Z = %d \n(Should be 0)\n", ret->description.Z);
    printf("respcode = %d \n(Should be error:1)\n", ret->description.resp_code);

    close(sockfd);
}

void init() {
    server_size = sizeof(struct sockaddr_in);

    // As per man page, set protocol to 0 (generic IP)
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		fprintf(stderr, "Failed to create socket.");
		exit(1);
	}
}

void testLabelSerdes(char* name) {
    uint8_t* label;
    createLabelFromChar(name, &label);
    int i = 0;
    int len = strlen(name);
    for (; i <= len; i++) {
        printf("%02x ", label[i]);
    }
    printf("\n");

    char str[255];
    createCharFromLabel(label, str);
    printf("%s\n", str);
}

int main(int argc, char *argv[]) {
    //Take the input string, pass to the parseLabel method.
    char name[20] = "www.google.com."; //For now, simply use a hard-coded domain.
    //Grabbed from A1 udp client
    char* root_name = getNextRootServer();

    init();
    queryForNameAt(name, root_name);
    //testLabelSerdes(name);
}
