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

const int ROOT_COUNT = 14;
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

struct STACK_ELE** stack;
int stackSize = 0;

int sockfd;
socklen_t server_size;
int cur_root_server = 0;

struct PTR_VAL {
    int len;
    int pointerStart;
    int start;
};

char* getNextRootServer() {
    //TODO: Possibly use this function for any list of IP addresses to try
    if (cur_root_server == ROOT_COUNT) {
        return NULL;
    }
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
    (*label) = (uint8_t*) malloc((sizeof(uint8_t) * strLen) + 1);
    if (*label == NULL) {
        fprintf(stderr, "Malloc failed.\n");
        exit(2);
    }

    uint8_t* itr = (*label);

    //Initialize first label length.
    (*itr) = 0;
    itr++;
    cur = *(str + counter);
    while (1 && strLen > 0) {
        if (cur == '\0') {
            // Go back and set size
            if (size == 0) {
                (*(itr - 1)) = size;
            } else {
                (*(itr - size - 1)) = size;
                // Add zero length for label termination
                *(itr) = 0;
            }
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

uint8_t getUint8(char* buf) {
	uint8_t ret;
	memcpy(&ret, buf, sizeof(uint8_t));
	return ret;
}

void parseLabel(char* buf, int* bytesParsed, struct PTR_VAL* ptrs, int* ptr_count, int* ptr_size, int recurse, int follow_ptr) {
    int start = *bytesParsed;
    while (1) {
        //Get size of label
        uint8_t curSize = (uint8_t) *(buf + *bytesParsed);

        // We have a pointer
        if ((curSize & 0xC0) == 0xC0 && follow_ptr == 1) {
            uint16_t ptr = 0;
            memcpy(&ptr, buf + *bytesParsed, sizeof(uint16_t));
            ptr = ntohs(ptr);
            ptr = ptr & 0x3FFF;

            int end = ptr;
            // Save the start of the pointer so that the pointer is removed from the copy
            struct PTR_VAL* cur = ptrs + *ptr_count;
            cur->pointerStart = *bytesParsed;
            cur->start = ptr;
            (*ptr_count)++;

            //XXX: There might be a bug here if we go recursive, but I need an example to verify.
            parseLabel(buf, &end, ptrs, ptr_count, ptr_size, 1, 1);

            cur->len = end - ptr;

            *ptr_size += end - ptr;
            // We parsed the pointer
            if (recurse == 0) {
                *bytesParsed += 2;
            }

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
            memcpy(dest + destItr, buf + srcItr, ptrs[j].pointerStart - srcItr);
            destItr += ptrs[j].pointerStart - srcItr;
            srcItr += (ptrs[j].pointerStart - srcItr);
        }

        memcpy(dest + destItr, buf + ptrs[j].start, ptrs[j].len);
        destItr += ptrs[j].len;
    }

    if (destItr < total_len) {
        memcpy(dest + destItr, buf + srcItr, total_len - destItr);
    }
}

void parseA(void** dest, char* src, uint16_t len) {
	*dest = malloc(sizeof(struct RR_A));
	memcpy(*dest, src, len);
}

void parseAAAA(void** dest, char* src, uint16_t len) {
	*dest = malloc(sizeof(struct RR_AAAA));
	memcpy(*dest, src, len);
}

void parseRdLabel(void** dest, char* src, int *bytesParsed, int follow_ptr) {
	struct PTR_VAL ptrs[128];

	int startPos = *bytesParsed;
	int ptrCount = 0;
	int ptrSize = 0;

	parseLabel(src, bytesParsed, ptrs, &ptrCount, &ptrSize, 0, follow_ptr);

	// Populate name
	// Size breakdown:
	// No pointer label = bytesParsed - startPos
	// Each pointer has 2 bytes not copied
	// The total number of pointers has ptrSize bytes
	int labelSize = ((*bytesParsed - startPos) - (ptrCount)) + ptrSize;
	*dest = (uint8_t *) malloc(labelSize + 1);
	copyLabel(src, *dest, startPos, ptrs, ptrCount, ptrSize, labelSize);
}

void parseCNAME(void** dest, char* src, int bytesParsed) {
	*dest = malloc(sizeof(struct RR_CNAME));
	struct RR_CNAME *s = (struct RR_CNAME*)*dest;
	parseRdLabel((void**)&(s->name), src, &bytesParsed, 1);
}

void parseNS(void** dest, char* src, int bytesParsed) {
	*dest = malloc(sizeof(struct RR_NS));
	struct RR_NS *s = (struct RR_NS*)*dest;
	parseRdLabel((void**)&(s->name), src, &bytesParsed, 1);
}

void parseSOA(void** dest, char* src, int bytesParsed) {
	*dest = malloc(sizeof(struct RR_SOA));
	struct RR_SOA *s = (struct RR_SOA*)*dest;
	parseRdLabel((void**)&(s->mname), src, &bytesParsed, 1);
	parseRdLabel((void**)&(s->rname), src, &bytesParsed, 1);

	s->serial = getUint32(src + bytesParsed);
	bytesParsed += sizeof(uint32_t);

	s->refresh_interval = getUint32(src + bytesParsed);
	bytesParsed += sizeof(uint32_t);

	s->retry_interval = getUint32(src + bytesParsed);
	bytesParsed += sizeof(uint32_t);

	s->expire_interval = getUint32(src + bytesParsed);
	bytesParsed += sizeof(uint32_t);

	s->minimum_ttl = getUint32(src + bytesParsed);
	bytesParsed += sizeof(uint32_t);
}

void parseRRSIG(void** dest, char* src, int bytesParsed, uint16_t len) {
	*dest = malloc(sizeof(struct RR_SOA));
	struct RR_SIG *s = (struct RR_SIG*)*dest;
	int start = bytesParsed;

	s->type_covered = getUint16(src + bytesParsed);
	bytesParsed += sizeof(uint16_t);

	s->algorithm = getUint8(src + bytesParsed);
	bytesParsed += sizeof(uint8_t);

	s->labels = getUint8(src + bytesParsed);
	bytesParsed += sizeof(uint8_t);

	s->ttl = getUint32(src + bytesParsed);
	bytesParsed += sizeof(uint32_t);

	s->sig_expiration = getUint32(src + bytesParsed);
	bytesParsed += sizeof(uint32_t);

	s->sig_inception = getUint32(src + bytesParsed);
	bytesParsed += sizeof(uint32_t);

	s->key_tag = getUint16(src + bytesParsed);
	bytesParsed += sizeof(uint16_t);

	parseRdLabel((void**)&(s->signer_name), src, &bytesParsed, 0);

	// Assume signature is remaining bytes.
	memcpy(&s->signature, src + bytesParsed, bytesParsed - start);
}

void parseRdata(void** dest, char* src, int bytesParsed, RR_TYPE type, uint16_t len) {
	if (type == MESSAGE_QTYPE_A) {
		parseA(dest, src + bytesParsed, len);
	} else if (type == MESSAGE_QTYPE_AAAA) {
		parseAAAA(dest, src + bytesParsed, len);
	} else if (type == MESSAGE_QTYPE_CNAME) {
		parseCNAME(dest, src, bytesParsed);
	} else if (type == MESSAGE_QTYPE_NS) {
		parseNS(dest, src, bytesParsed);
	} else if (type == MESSAGE_QTYPE_SOA) {
		parseSOA(dest, src, bytesParsed);
	} else if (type == MESSAGE_QTYPE_RRSIG) {
		parseRRSIG(dest, src, bytesParsed, len);
	} else {
		*dest = malloc(len);
		memcpy(*dest, src, len);
	}
}

void getQuestion(struct MESSAGE_QUESTION** question, int count, int *bytesParsed, char* buf) {
    if (count > 0) {
    	*question = (struct MESSAGE_QUESTION*)malloc(sizeof(struct MESSAGE_QUESTION) * count);
    	int i = 0;
    	//Names may only be max of 255 octets long, with worst case scenario being that
    	// each label is only one byte long (meaning two bytes), which is 127.5, make it safe with 128
    	struct PTR_VAL ptrs[255];
    	for (i = 0; i < count; i++) {
    		struct MESSAGE_QUESTION *newMsg = *question + i;
    		int startPos = *bytesParsed;
    		int ptrCount = 0;
    		int ptrSize = 0;

    		parseLabel(buf, bytesParsed, ptrs, &ptrCount, &ptrSize, 0, 1);

    		// Populate label
    		// Size breakdown:
    		// No pointer label = bytesParsed - startPos
    		// Each pointer has 2 bytes not copied
    		// The total number of pointers has ptrSize bytes
    		int labelSize = ((*bytesParsed - startPos) - (ptrCount)) + ptrSize;
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

    		parseLabel(buf, bytesParsed, ptrs, &ptrCount, &ptrSize, 0, 1);

    		// Populate name
    		// Size breakdown:
    		// No pointer label = bytesParsed - startPos
    		// Each pointer has 2 bytes not copied
    		// The total number of pointers has ptrSize bytes
    		int labelSize = ((*bytesParsed - startPos) - (ptrCount)) + ptrSize;
    		newRecord->name = (uint8_t *) malloc(labelSize);
    		copyLabel(buf, newRecord->name, startPos, ptrs, ptrCount, ptrSize, labelSize);

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
    		parseRdata(&(newRecord->rd_data), buf, *bytesParsed, ntohs(newRecord->type), rdLen);
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

void printNS(char* name, char* rrType, char* class, uint16_t ttl, uint16_t rdLen, void* rd_data) {
	char ns[255];
	createCharFromLabel(((struct RR_CNAME*) (rd_data))->name, ns);
	printf("%s %u %s %s %u %s\n", name, ttl, "IN", rrType, rdLen, ns);
}

void printA(char* name, char* rrType, char* class, uint16_t ttl, uint16_t rdLen, void* rd_data) {
	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, rd_data, ip, INET_ADDRSTRLEN);
	printf("%s %u %s %s %u %s\n", name, ttl, "IN", rrType, rdLen, ip);
}

void printAAAA(char* name, char* rrType, char* class, uint16_t ttl, uint16_t rdLen, void* rd_data) {
	char ip[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, rd_data, ip, INET6_ADDRSTRLEN);
	printf("%s %u %s %s %u %s\n", name, ttl, "IN", rrType, rdLen, ip);
}

void printRr(struct MESSAGE_RESOURCE_RECORD* rr, uint16_t count, char* title) {
    count = ntohs(count);
    printf("%s", title);
    printf("FOUND %u records\n", count);
    int i = 0;

    for (;i < count; i++) {
        struct MESSAGE_RESOURCE_RECORD* cur = rr + i;
        char name[255];
        char rrType[20];
        createCharFromLabel(cur->name, name);
        getRrTypeStr(ntohs(cur->type), rrType);
        RR_TYPE type = ntohs(cur->type);
        uint32_t ttl = ntohl(cur->ttl);
        uint16_t rdLen = ntohs(cur->rdlength);
        void* rd_data = cur->rd_data;

        if (type == MESSAGE_QTYPE_NS) {
        	printNS(name, rrType, "IN", ttl, rdLen, rd_data);
        } else if (type == MESSAGE_QTYPE_A) {
        	printA(name, rrType, "IN", ttl, rdLen, rd_data);
        } else if (type == MESSAGE_QTYPE_AAAA) {
        	printAAAA(name, rrType, "IN", ttl, rdLen, rd_data);
        } else {
        	printf("%s %u %s %s %u\n", name, ttl, "IN", rrType, rdLen);
        }
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

int sendQuery(char* hostToResolve, char* dns_server, RR_TYPE query_type, struct sockaddr_in* server) {
    struct DNS_MESSAGE test_message;
    test_message.header.id = htons(10);
    test_message.header.additional_count = htons(0);
    test_message.header.answer_count = htons(0);
    test_message.header.description = htons(0);
    test_message.header.nameserver_count = htons(0);
    test_message.header.question_count = htons(1);

    test_message.question = (struct MESSAGE_QUESTION*) malloc(sizeof(struct MESSAGE_QUESTION));

    struct MESSAGE_QUESTION* question = test_message.question;
    uint8_t* name_label;
    createLabelFromChar(hostToResolve, &name_label);
    question->qname = name_label;
    question->qclass = htons(MESSAGE_QCLASS_IN);
    question->qtype = htons(query_type);

    fprintf(stderr, "Trying DNS server %s\n", dns_server);
    //printDnsMessage(&test_message);
    sendDnsMessage(&test_message, server);
    printf("done sending... wait for reply!\n");
    return 0;
}

int getResponse(struct DNS_MESSAGE* response, struct sockaddr_in* server) {
    char* data = malloc(sizeof(char) * MAX_UDP_SIZE);
    int recvLen = sizeof(server);
    
    //Setup for client timeout
	struct timeval  t;
	t.tv_sec  = 5;
	t.tv_usec = 0;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t)) < 0) {
		fprintf(stderr, "Error setting timeout condition.\n");
		return 8;
	}
    int bytesReceived = recvfrom(sockfd, data, MAX_UDP_SIZE, 0, (struct sockaddr*) (&(server)), &recvLen);
    if (bytesReceived < 0) {
        fprintf(stderr, "Error receiving packet: %s\n", strerror(errno));
        return RET_INVALID_IP; //Could also be a "Service temporarily unavailable" error.
    }

    int bytesParsed = 0;
    if (bytesReceived >= sizeof(struct MESSAGE_HEADER)) {
    	memcpy(&(response->header), data, sizeof(struct MESSAGE_HEADER));
    	bytesParsed += sizeof(struct MESSAGE_HEADER);
    }

    struct MESSAGE_HEADER_EXT h;
    unpackExtendedMessageHeader(&(response->header), &h);

    getQuestion(&(response->question), h.question_count, &bytesParsed, data);
    getResourceRecord(&(response->answer), h.answer_count, &bytesParsed, data);
    getResourceRecord(&(response->authority), h.nameserver_count, &bytesParsed, data);
    getResourceRecord(&(response->additional), h.additional_count, &bytesParsed, data);
    free(data);
    return 0;
}

int initServer(struct sockaddr_in* server, char* dns_server) {
    // Setup server to be queried
    server->sin_family = AF_INET;
    server->sin_port = htons(DNS_PORT);
    // Clear out the data.
    memset(server->sin_zero, '\0', sizeof(server->sin_zero));
    int result = inet_pton(AF_INET, dns_server, &(server->sin_addr.s_addr));
    if (result != 1) {
    	if (result == 0) {
    		fprintf(stderr, "Invalid IP passed in.\n");
    	} else if (result < 0) {
    		fprintf(stderr, "Unable to parse IP address: %s\n", strerror(errno));
    	}
    	return RET_INVALID_IP;
    }
    return 0;
}

int assignAnswer(struct MESSAGE_RESOURCE_RECORD* dest, struct MESSAGE_RESOURCE_RECORD* list, uint16_t listCount, RR_TYPE* query_types, int typeCount, struct MESSAGE_RESOURCE_RECORD** answer) {
	int i = 0;

	for (; i < listCount; i++) {
		struct MESSAGE_RESOURCE_RECORD* cur = (list + i);

		int j = 0;
		for (; j < typeCount; j++) {
			if (ntohs(cur->type) == query_types[j]) {
				memcpy(dest, cur, sizeof(struct MESSAGE_RESOURCE_RECORD));
				fprintf(stderr, "Found an answer to NS query.\n");
				return RET_FOUND_ANSWER;
			}
		}

		if (ntohs(cur->type) == MESSAGE_QTYPE_CNAME) {
			char cname[1024];
			createCharFromLabel(((struct RR_CNAME*)cur->rd_data)->name, cname);
			fprintf(stderr, "Found CNAME: %s\n", cname);
			return loopThroughList(cname, ROOT_IP, ROOT_COUNT, MESSAGE_QTYPE_AAAA, answer);
		}

		if (ntohs(cur->type) == MESSAGE_QTYPE_SOA) {
			fprintf(stderr, "Not sure what to do about the SOA yet. \n");
			return RET_ANSWER_NOT_FOUND;
		}
	}

	return RET_ANSWER_NOT_FOUND;
}

int checkAuthoritativeAnswer(struct DNS_MESSAGE* response, RR_TYPE query_type, struct MESSAGE_RESOURCE_RECORD** answer) {
	printf("We got a definitive answer.\n");
	struct MESSAGE_HEADER_EXT ret;
	unpackExtendedMessageHeader(&(response->header), &ret);
	// TODO: determine if the record type being searched for was found.

	// likely parsing ns records.
	if (query_type == MESSAGE_QTYPE_A) {
		*answer = (struct MESSAGE_RESOURCE_RECORD*) malloc(sizeof(struct MESSAGE_RESOURCE_RECORD));
		RR_TYPE types[1];
		types[0] = MESSAGE_QTYPE_A;
		return assignAnswer(*answer, response->answer, ret.answer_count, types, 1, answer);

	// likely parsing the AAAA with RRSIG
	} else if (query_type == MESSAGE_QTYPE_AAAA) {
		*answer = (struct MESSAGE_RESOURCE_RECORD*) malloc(sizeof(struct MESSAGE_RESOURCE_RECORD) * 2);
		RR_TYPE types[2];
		types[0] = MESSAGE_QTYPE_AAAA;
		types[1] = MESSAGE_QTYPE_RRSIG;
		
		return assignAnswer(*answer, response->answer, ret.answer_count, types, 2, answer);
	} else {
		fprintf(stderr, "Not sure what is being queried for.\n");
	}
	return RET_ANSWER_NOT_FOUND;
}

int queryForNameAt(char* name, char* root_name, RR_TYPE query_type, struct MESSAGE_RESOURCE_RECORD** answer) {
    if (addToStack(name, root_name) != RET_NO_RECURSION_FOUND) {
        fprintf(stderr, "Attempted infinite recursion. Aborted.\n");
        return RET_ATTEMPTED_RECURSE;
    }

    struct sockaddr_in server;
    int serverRet = 0;
    if ((serverRet = initServer(&server, root_name)) != 0) {
        printf("Invalid dns server.\n");
        return serverRet;
    }
    sendQuery(name, root_name, query_type, &server);

    struct DNS_MESSAGE *response = (struct DNS_MESSAGE *) malloc(sizeof(struct DNS_MESSAGE));
    getResponse(response, &server);

    struct MESSAGE_HEADER_EXT ret;
    unpackExtendedMessageHeader(&(response->header), &ret);
    //printDnsMessage(response);

    // Start checking
    if (ret.description.qr_type == DNS_QR_TYPE_QUERY) {
    	fprintf(stderr, "Expected a response, received a query. Aborting.\n");
    	return RET_INVALID_RESPONSE;
    } else if (ret.description.auth_answer == DNS_AA_TRUE &&
    		ret.description.resp_code == DNS_RCODE_NAME_ERROR) {
    	fprintf(stderr, "No such name.\n");
    	return RET_NO_SUCH_NAME;
    } else if (ret.description.resp_code != DNS_RCODE_NOERROR) {
    	fprintf(stderr, "Invalid response type.\n");
    	return RET_INVALID_RESPONSE;
    }

    // We have a definitive answer, let's print stuff out
    if (ret.description.auth_answer == DNS_AA_TRUE) {
    	if (ret.answer_count > 0) {
    		return checkAuthoritativeAnswer(response, query_type, answer);
    	} else {
    		return RET_NO_SUCH_NAME;
    	}
    } else if (ret.additional_count > 0 || ret.answer_count > 0) {
        // Let's hope that we got some IP addresses resolved for us.
        int result = loopThroughRRs(name, root_name, response->additional, ret.additional_count, query_type, answer);

        if (result < 0 || result == RET_FOUND_ANSWER) {
        	return result;
        } else {
        	// We didn't find the answer in the additional section
        	// This could be because the servers were down, no resolutions were provided,
        	// or the answer didn't exist. Regardless we're now going to check the answer
        	// section.
        	result = loopThroughRRs(name, root_name, response->answer, ret.answer_count, query_type, answer);

        }
        //TODO: Should we attempt to parse the authority section here?
    } else {
    	printf("No responses.\n");
    }

    return RET_ANSWER_NOT_FOUND;
}

int loopThroughList(char* name, char** list, int count, RR_TYPE query_type, struct MESSAGE_RESOURCE_RECORD** answer) {
	int i = 0;
	int ret = RET_ANSWER_NOT_FOUND;
	for (; i < count; i++) {
		char* cur = list[i];
		ret = queryForNameAt(name, cur, query_type, answer);
		if (ret <= RET_FOUND_ANSWER) {
			break;
		}
	}

	return ret;
}

int loopThroughRRs(char* name, char* last_root, struct MESSAGE_RESOURCE_RECORD* answers, uint16_t count, RR_TYPE query_type, struct MESSAGE_RESOURCE_RECORD** answer) {
	int i = 0;
	for (; i < count; i++) {
		struct MESSAGE_RESOURCE_RECORD* rr = answers + i;
		RR_TYPE type = ntohs(rr->type);
		char* new_root = NULL;
		char ip[INET_ADDRSTRLEN];

		// Need to resolve NS records
		if (type == MESSAGE_QTYPE_NS) {
			struct MESSAGE_RESOURCE_RECORD* parsedResponse;
			char ns[1024];
			createCharFromLabel(((struct RR_NS*) rr->rd_data)->name, ns);

			// First make sure we're not infinitely recursing.
			int nsLen = strlen(ns);
			int lrLen = strlen(last_root);

			if (nsLen >= lrLen) {
				if (strcmp(last_root, ns + (nsLen - lrLen)) == 0) {
					fprintf(stderr, "Attempted infinite recursion (1). Skipping.\n");
					// Skip to next one in list.
					continue;
				}
			} else if (lrLen > nsLen) {
				if (strcmp(ns, last_root + (lrLen - nsLen)) == 0) {
					fprintf(stderr, "Attempted infinite recursion (2). Skipping.\n");
					// Skip to next one in list.
					continue;
				}
			}

			int result = loopThroughList(ns, ROOT_IP, ROOT_COUNT, MESSAGE_QTYPE_A, &parsedResponse);

			if (result == RET_FOUND_ANSWER) {
				inet_ntop(AF_INET, parsedResponse->rd_data, ip, INET_ADDRSTRLEN);
			} else {
				continue;
			}
		}

		// We got lucky with an A record!
		if (type == MESSAGE_QTYPE_A) {
			inet_ntop(AF_INET, rr->rd_data, ip, INET_ADDRSTRLEN);
		}

		if (ip != NULL) {
			int result = queryForNameAt(name, ip, query_type, answer);
			if (result <= RET_FOUND_ANSWER) {
				return result;
			}
		}
	}

	return RET_ANSWER_NOT_FOUND;
}

int addToStack(char *name, char* ip) {
    int i = 0;
    struct STACK_ELE *match = NULL;

    // See if we have a match already
    for (; i < stackSize; i++) {
        struct STACK_ELE *cur = stack[i];
        if (strcmp(name, cur->name) == 0) {
            int j = 0;
            match = cur;
            for (; j < cur->ipSize; j++) {
                if (strcmp(cur->ips[j], ip) == 0) {
                    return RET_ATTEMPTED_RECURSE;
                }
            }
            break;
        }
    }

    // No match, let's add in the new record, this is the least efficient
    // method possible but it works.
    if (match == NULL) {
        struct STACK_ELE **newStack = (struct STACK_ELE**) malloc(sizeof(struct STACK_ELE *) * (stackSize + 1));
        struct STACK_ELE *newMatch = (struct STACK_ELE*) malloc(sizeof(struct STACK_ELE));
        newMatch->name = malloc(sizeof(char) * strlen(name) + 1);
        memcpy(newMatch->name, name, strlen(name));

        for (i=0; i < stackSize; i++) {
            newStack[i] = stack[i];
        }
        free(stack);

        newStack[stackSize] = newMatch;
        ++stackSize;
        match = newMatch;
        stack = newStack;
    }

    char* newIp = (char*) malloc(sizeof(char) * strlen(ip) + 1);
    memcpy(newIp, ip, strlen(ip) + 1);
    char** newIps = (char**) malloc(sizeof(char *) * (match->ipSize + 1));

    for (i = 0; i < match->ipSize; i++) {
        newIps[i] = match->ips[i];
    }
    free(match->ips);

    newIps[match->ipSize] = newIp;
    match->ipSize++;
    match->ips = newIps;
    return RET_NO_RECURSION_FOUND;
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

void strtolower(char str[]) {
	int i;
	for (i = 0; str[i] != '\0'; ++i) {
		if (str[i] >= 'A' && str[i] <= 'Z') {
			str[i] = str[i] + 'a' - 'A';
		}
 	}
}

void testParseLabel() {
    char* buf = (char*) malloc(sizeof(char) * 1000);
    struct PTR_VAL ptrs[255];
    struct MESSAGE_RESOURCE_RECORD rr;
    uint32_t ip = htonl(0x0);
    rr.ttl = 10000;
    rr.type = htons(MESSAGE_QTYPE_A);
    rr.rdlength = htons(32);
    rr.rd_data = (void *) &ip;
    int ptrCount = 0;
    int ptrSize = 0;
    int bytesParsed = 0;

    buf[0] = 0x03; // 3
    buf[1] = 0x77; // w
    buf[2] = 0x77; // w
    buf[3] = 0x77; // w
    buf[4] = 0x01; // 1
    buf[5] = 0x61; // a
    buf[6] = 0x03; // 3
    buf[7] = 0x63; // c
    buf[8] = 0x6f; // o
    buf[9] = 0x6d; // m
    buf[10] = 0x00; // 0
    buf[11] = 0x03; // 3
    buf[12] = 0x61; // a
    buf[13] = 0x62; // b
    buf[14] = 0x63; // c
    buf[15] = 0xC0; // ptr
    buf[16] = 0x06; // 6
    buf[17] = 0x01; // 1
    buf[18] = 0x7a; // z
    buf[19] = 0xC0; // ptr
    buf[20] = 0x0b; // 11
    buf[21] = 0xC0; // ptr
    buf[22] = 0x11; // 17

    bytesParsed = 21;
    int startPos = bytesParsed;
    parseLabel(buf, &bytesParsed, ptrs, &ptrCount, &ptrSize, 0, 1);

    int labelSize = ((bytesParsed - startPos) - (ptrCount)) + ptrSize;
    rr.name = (uint8_t *) malloc(labelSize);
    copyLabel(buf, rr.name, startPos, ptrs, ptrCount, ptrSize, labelSize);

    printRr(&rr, htons(1), "TEST");
}

int main(int argc, char *argv[]) {
    char *name = (char*)malloc(sizeof(char) * 1024);

    if (argc == 1) {
    	memcpy(name, "www.google.com.", 16);
        //name = "www.google.com."; //For now, simply use a hard-coded domain.
    } else if (argc == 2) {
		if (strlen(argv[1]) >= 1024) {
			fprintf(stderr, "Invalid hostname, too long.\n");
			return 1;
		}
		memcpy(name, argv[1], strlen(argv[1]) + 1);
        //name = argv[1];
        strtolower(name);
        // Append period if necessary
        if (name[strlen(name) - 1] != '.') {
        	int len = strlen(name);
        	name[len] = '.';
        	name[len + 1] = '\0';
        }
    } else {
        printf("Invalid argument.\n");
        return 1;
    }

    init();
    struct MESSAGE_RESOURCE_RECORD* answer;
    int ret = loopThroughList(name, ROOT_IP, ROOT_COUNT, MESSAGE_QTYPE_AAAA, &answer);
    if (ret == RET_FOUND_ANSWER) {
    	printRr(answer, 256, "FOUND ULTIMATE ANSWER\n");
    } else if (ret == RET_NO_SUCH_NAME) {
    	printf("No record of that type.\n");
    } else {
    	printf("No record found.\n");
    }
    close(sockfd);
    free(name);

    return ret;
    //testLabelSerdes(name);
    //testParseLabel();
}
