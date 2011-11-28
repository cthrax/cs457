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

struct PTR_VAL {
    int len;
    int pointerStart;
    int start;
};

char* getNextRootServer() {
    //TODO: Need some protection for going out of bounds
    //TODO: Possibly use this function for any list of IP addresses to try
	return ROOT_IP[cur_root_server++];
}

//ret assumed already allocated.
void unpackExtendedMessageHeader(struct MESSAGE_HEADER* header,
        struct MESSAGE_HEADER_EXT* ret) {
    if (ret == NULL || header == NULL) {
        fprintf(stderr, "Uninitialized return struct.");
        return;
    }
    ret->id = ntohs(header->id);
    ret->question_count = ntohs(header->question_count);
    ret->answer_count = ntohs(header->answer_count);
    ret->nameserver_count = ntohs(header->nameserver_count);
    ret->additional_count = ntohs(header->additional_count);

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

    ret->description = htons(header->description.qr_type
            | header->description.opcode
            | header->description.auth_answer
            | header->description.trunc
            | header->description.recurse_desired
            | header->description.recurse_available
            | header->description.Z
            | header->description.resp_code);
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

void appendToBuffer(char** buf, void* data, int size, int* total_size) {
	if (*total_size + size > MAX_UDP_SIZE) {
		fprintf(stderr, "Packet too large. Aborting.");
		exit(1);
	}

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
    for (i = 0; i < ntohs(message->header.question_count); i++) {
    	struct MESSAGE_QUESTION* cur = message->question + i;
    	// Append name
    	appendToBuffer(&itr, cur->qname, labelLen(cur->qname), &packet_size);
    	// Append type
    	appendToBuffer(&itr, &(cur->qtype), sizeof(cur->qtype), &packet_size);
    	// Append class
    	appendToBuffer(&itr, &(cur->qclass), sizeof(cur->qclass), &packet_size);
    }
    sendNumBytes(buf, packet_size, server);
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
	return type;
}

QCLASS getClassType(char* buf) {
	QCLASS class;
	memcpy(&class, buf, sizeof(QCLASS));
	return class;
}

uint32_t getUint32(char* buf) {
	uint32_t ret;
	memcpy(&ret, buf, sizeof(uint32_t));
	return ret;
}

uint16_t getUint16(char* buf) {
	uint16_t ret;
	memcpy(&ret, buf, sizeof(uint16_t));
	return ret;
}

void parseLabel(char* buf, int* bytesParsed, struct PTR_VAL* ptrs, int* ptr_count, int* ptr_size) {
    while (1) {
        //Get size of label
        uint8_t curSize = (uint8_t) *(buf + *bytesParsed);

        // We have a pointer
        if ((curSize & 0xC0) == 0xC0) {
            uint16_t ptr = 0;
            memcpy(&ptr, buf + *bytesParsed, sizeof(uint16_t));
            ptr = ntohs(ptr);
            ptr = ptr & 0x3FFF;

            //Pointer points to just before the start.
            int end = ptr;
            int startPtrCnt = *ptr_count;

            //XXX: There might be a bug here if we go recursive, but I need an example to verify.
            // It might not be worth the effort, because it's unclear if recursive compression occurs, though it most likely does.
            parseLabel(buf, &end, ptrs, ptr_count, ptr_size);

            // Save the start of the pointer so that the pointer is removed from the copy
            struct PTR_VAL* cur = ptrs + *ptr_count;
            cur->pointerStart = *bytesParsed;
            cur->len = end - ptr;
            cur->start = ptr;

            *ptr_size += end - ptr;
            (*ptr_count)++;
            // We parsed the pointer
            *bytesParsed += 2 * (*ptr_count - startPtrCnt);

            // By definition, a pointer is the end.
            break;
        } else {
            *bytesParsed += 1;
            if (curSize == 0) {
                break;
            }
            *bytesParsed += curSize;
        }
    }
}

void copyLabel(char* buf, uint8_t* dest, int start, struct PTR_VAL* ptrs, int ptrCount, int ptrSize, int total_len) {
    int j = 0;
    int destItr = 0;
    int srcItr = start;
    for (; j < ptrCount; j++) {
        if (srcItr < ptrs[j].pointerStart) {
            memcpy(dest + destItr, buf + srcItr, ptrs[j].start - start);
            destItr += ptrs[j].start - srcItr;
            srcItr += (ptrs[j].start - srcItr) + 2;

        } else {
            memcpy(dest + destItr, buf + ptrs[j].start, ptrs[j].len);
            destItr += ptrs[j].len;
        }
    }

    if (destItr < total_len) {
        memcpy(dest + destItr, buf + srcItr, total_len - destItr);
    }
}

void getQuestion(struct MESSAGE_QUESTION** question, int count, int *bytesParsed, char* buf) {
    if (count > 0) {
    	*question = (struct MESSAGE_QUESTION*)malloc(sizeof(struct MESSAGE_QUESTION) * count);
    	int i = 0;
    	//Names may only be max of 255 octets long, with worst case scenario being that
    	// each label is only one byte long (meaning two bytes), which is 127.5, make it safe with 128
    	struct PTR_VAL ptrs[128];
    	for (i = 0; i < count; i++) {
    		struct MESSAGE_QUESTION *newMsg = *question + i;
    		int startPos = *bytesParsed;
    		int ptrCount = 0;
    		int ptrSize = 0;

    		parseLabel(buf, bytesParsed, ptrs, &ptrCount, &ptrSize);

    		// Populate label
    		// Size breakdown:
    		// No pointer label = bytesParsed - startPos
    		// Each pointer has 2 bytes not copied
    		// The total number of pointers has ptrSize bytes
    		int labelSize = ((*bytesParsed - startPos) - (ptrCount * 2)) + ptrSize;
    		newMsg->qname = (uint8_t *) malloc(labelSize);
    		copyLabel(buf, newMsg->qname, startPos, ptrs, ptrCount, ptrSize, labelSize);

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
    	*record = (struct MESSAGE_RESOURCE_RECORD*) malloc(sizeof(struct MESSAGE_RESOURCE_RECORD) * count);
    	int i = 0;
    	//Names may only be max of 255 octets long, with worst case scenario being that
    	// each label is only one byte long (meaning two bytes), which is 127.5, make it safe with 128
    	struct PTR_VAL ptrs[128];
    	for (i = 0; i < count; i++) {
    		struct MESSAGE_RESOURCE_RECORD *newRecord = *record + i;
    		int startPos = *bytesParsed;
    		int ptrCount = 0;
    		int ptrSize = 0;

    		parseLabel(buf, bytesParsed, ptrs, &ptrCount, &ptrSize);

    		// Populate name
    		// Size breakdown:
    		// No pointer label = bytesParsed - startPos
    		// Each pointer has 2 bytes not copied
    		// The total number of pointers has ptrSize bytes
    		int labelSize = ((*bytesParsed - startPos) - (ptrCount * 2)) + ptrSize;
    		newRecord->name = (uint8_t *) malloc(labelSize); copyLabel(buf, newRecord->name, startPos, ptrs, ptrCount, ptrSize, labelSize);
    		char t[1024];
    		createCharFromLabel(newRecord->name, t);

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
    		uint16_t rdLen = ntohs(newRecord->rdlength);

    		// Populate RD Data
    		newRecord->rd_data = malloc(rdLen);
    		memcpy(newRecord->rd_data, buf + *bytesParsed, rdLen);
    		*bytesParsed += rdLen;

    		*((*record) + i) = *newRecord;
    	}
    }
}

void printHeader(struct MESSAGE_HEADER* header) {
    struct MESSAGE_HEADER_EXT ext;
    unpackExtendedMessageHeader(header, &ext);
    struct MESSAGE_DESCRIPTION* desc = &(ext.description);

    printf("=============HEADER=================\n");
    printf("id:                            %5d\n", ext.id);
    printf("description:                   %5d\n", ntohs(header->description));
    printf("  +qr:                 %02x\n", desc->qr_type >> 15);
    printf("  +opcode:             %02x\n", desc->opcode >> 11);
    printf("  +auth_answer:        %02x\n", desc->auth_answer >> 10);
    printf("  +truncated:          %02x\n", desc->trunc >> 9);
    printf("  +rec_desired:        %02x\n", desc->recurse_desired >> 8);
    printf("  +rec_avail:          %02x\n", desc->recurse_available >> 7);
    printf("  +Z:                  %02x\n", desc->Z >> 4);
    printf("  +resp_code:          %02x\n", desc->resp_code);
    printf("question count:                %5d\n", ext.question_count);
    printf("answer count:                  %5d\n", ext.answer_count);
    printf("nameserver count:              %5d\n", ext.nameserver_count);
    printf("additional count:              %5d\n", ext.additional_count);
}

void getRrTypeStr(RR_TYPE type, char* dest) {
    // Stupid C, apparently uint16_t doesn't boil down to an int
    // so a switch won't work.
    if (type == MESSAGE_QTYPE_A) {
        sprintf(dest, "A");
    } else if (type == MESSAGE_QTYPE_NS) {
        sprintf(dest, "NS");
    } else if (type == MESSAGE_QTYPE_CNAME) {
        sprintf(dest, "CNAME");
    } else if (type == MESSAGE_QTYPE_STAR) {
        sprintf(dest, "*", 1);
    } else if (type == MESSAGE_QTYPE_AAAA) {
        sprintf(dest, "AAAA", 4);
    } else {
        sprintf(dest, "UNK(%u)", type);
    }
}

void printQuestion(struct MESSAGE_QUESTION* question, uint16_t count) {
    count = ntohs(count);
    printf("=============QUESTION===============\n");
    printf("FOUND %u questions\n", count);
    int i = 0;

    for (; i < count; i++) {
        struct MESSAGE_QUESTION* cur = question + i;
        char name[255];
        char rrType[10];
        createCharFromLabel(cur->qname, name);
        getRrTypeStr(ntohs(cur->qtype), rrType);

        // We only use class IN
        printf("%s  %s  %s\n", name, rrType, "IN");
    }
}

void printRr(struct MESSAGE_RESOURCE_RECORD* rr, uint16_t count, char* title) {
    count = ntohs(count);
    printf("%s", title);
    printf("FOUND %u records\n", count);
    int i = 0;

    for (;i < count; i++) {
        struct MESSAGE_RESOURCE_RECORD* cur = rr + i;
        char name[255];
        char rrType[10];
        createCharFromLabel(cur->name, name);
        getRrTypeStr(ntohs(cur->type), rrType);
        printf("%s %u %s %s %u\n", name, ntohl(cur->ttl), "IN", rrType, ntohs(cur->rdlength));
    }
}

printAnswer(struct MESSAGE_RESOURCE_RECORD* rr, uint16_t count) {
    printRr(rr, count, "============ANSWER==================\n");
}

printAuthority(struct MESSAGE_RESOURCE_RECORD* rr, uint16_t count) {
    printRr(rr, count, "============AUTHORITY==================\n");
}

printAdditional(struct MESSAGE_RESOURCE_RECORD* rr, uint16_t count) {
    printRr(rr, count, "============ADDITIONAL==================\n");
}

void printDnsMessage(struct DNS_MESSAGE* message) {
    printHeader(&(message->header));
    printQuestion(message->question, message->header.question_count);
    printAnswer(message->answer, message->header.answer_count);
    printAuthority(message->authority, message->header.nameserver_count);
    printAdditional(message->additional, message->header.additional_count);
    printf("=============END=====================\n");
}

void queryForNameAt(char* name, char* root_name) {

    struct DNS_MESSAGE test_message;
    test_message.header.id = htons(10);
    test_message.header.additional_count = htons(0);
    test_message.header.answer_count = htons(0);
    test_message.header.description = htons(0);
    test_message.header.nameserver_count = htons(0);
    test_message.header.question_count = htons(1);

    struct MESSAGE_HEADER_EXT ext;
    unpackExtendedMessageHeader(&(test_message.header), &ext);
    ext.description.qr_type = DNS_QR_TYPE_QUERY;
    repackExtendedMessageHeader(&ext, &(test_message.header));

    test_message.question = (struct MESSAGE_QUESTION*) malloc(sizeof(struct MESSAGE_QUESTION));

    struct MESSAGE_QUESTION* question = test_message.question;
    uint8_t* name_label;
    createLabelFromChar(name, &name_label);
    question->qname = name_label;
    question->qclass = htons(MESSAGE_QCLASS_IN);
    question->qtype = htons(MESSAGE_QTYPE_AAAA);

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
    printDnsMessage(&test_message);
    sendDnsMessage(&test_message, &server);
    printf("done sending... wait for reply!\n");

    char* data = malloc(sizeof(char) * MAX_UDP_SIZE);
    socklen_t recvLen;

    // We'll see if it's the case, but I read online that UDP packets expect to send the whole packet in one go
    // I tried doing multiple reads and that fails.
    int bytesReceived = recvfrom(sockfd, data, MAX_UDP_SIZE, 0, (struct sockaddr*) (&(server)), &recvLen);
    if (bytesReceived < 0) {
        fprintf(stderr, "Error receiving packet: %s\n", strerror(errno));
        //TODO: Should we exit here, or is where we re-initiate the loop with the next server?
        exit(1);
    }

    int bytesParsed = 0;

    struct DNS_MESSAGE* response = (struct DNS_MESSAGE*) malloc(sizeof(struct DNS_MESSAGE));
    if (bytesReceived >= sizeof(struct MESSAGE_HEADER)) {
    	memcpy(&(response->header), data, sizeof(struct MESSAGE_HEADER));
    	bytesParsed += sizeof(struct MESSAGE_HEADER);
    }

    struct MESSAGE_HEADER_EXT* ret = malloc(sizeof(struct MESSAGE_HEADER_EXT));
    unpackExtendedMessageHeader(&(response->header), ret);

    fprintf(stderr, "Fetching response question. %u\n", bytesParsed);
    getQuestion(&response->question, ret->question_count, &bytesParsed, data);
    fprintf(stderr, "Fetching response answer. %u\n", bytesParsed);
    getResourceRecord(&response->answer, ret->answer_count, &bytesParsed, data);
    fprintf(stderr, "Fetching response authority. %u\n", bytesParsed);
    getResourceRecord(&response->authority, ret->nameserver_count, &bytesParsed, data);
    fprintf(stderr, "Fetching response additional. %u\n", bytesParsed);
    getResourceRecord(&response->additional, ret->additional_count, &bytesParsed, data);

    printDnsMessage(response);

    if (ret->description.qr_type == DNS_QR_TYPE_QUERY) {
    	fprintf(stderr, "Expected a response, received a query. Aborting.\n");
    	exit(1);
    }

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
