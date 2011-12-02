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

int sockfd;
socklen_t server_size;
int cur_root_server = 0;

struct PTR_VAL {
    int len;
    int pointerStart;
    int start;
};

void getIPV4addr(uint32_t* in, char* out)
{
    uint32_t addr = (*in);//ntohl breaks it?
   inet_ntop(AF_INET, &addr, out, INET_ADDRSTRLEN);
   printf("IP Addr: %s\n", out);
   /*uint8_t* ptr = in;
   uint8_t one = *ptr;
   ptr ++;
   uint8_t two = *ptr;
   ptr ++;
   uint8_t three = *ptr;
   ptr ++;
   uint8_t four = *ptr;
   sprintf(out, "%d.%d.%d.%d", (int)one,(int)three,(int)two, (int)four);*/
   //This method returns valid-looking IPV4 addresses, BUT, they simply don't work.
   //four.three.one.two *seems* to be the correct answer as it actually will allow me to get ANSWER sections.
   //I have *no* idea why this is so, but there you have it.
   // 46.1.192.102 actually responded, and gave an answer to google.com. . . 
   
}

char* getNextRootServer() {
    //TODO: Need some protection for going out of bounds
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

void parseLabel(char* buf, int* bytesParsed, struct PTR_VAL* ptrs, int* ptr_count, int* ptr_size, int recurse) {
    int start = *bytesParsed;
    while (1) {
        //Get size of label
        uint8_t curSize = (uint8_t) *(buf + *bytesParsed);

        // We have a pointer
        if ((curSize & 0xC0) == 0xC0) {
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
            parseLabel(buf, &end, ptrs, ptr_count, ptr_size, 1);

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
            memcpy(dest + destItr, buf + srcItr, ptrs[j].start - start);
            destItr += ptrs[j].start - srcItr;
            srcItr += (ptrs[j].start - srcItr);
        }

        memcpy(dest + destItr, buf + ptrs[j].start, ptrs[j].len);
        destItr += ptrs[j].len;
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
    	struct PTR_VAL ptrs[255];
    	for (i = 0; i < count; i++) {
    		struct MESSAGE_QUESTION *newMsg = *question + i;
    		int startPos = *bytesParsed;
    		int ptrCount = 0;
    		int ptrSize = 0;

    		parseLabel(buf, bytesParsed, ptrs, &ptrCount, &ptrSize, 0);

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

    		parseLabel(buf, bytesParsed, ptrs, &ptrCount, &ptrSize, 0);

    		// Populate name
    		// Size breakdown:
    		// No pointer label = bytesParsed - startPos
    		// Each pointer has 2 bytes not copied
    		// The total number of pointers has ptrSize bytes
    		int labelSize = ((*bytesParsed - startPos) - (ptrCount * 2)) + ptrSize;
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
        printf("%s %u %s %s %u \n", name, ntohl(cur->ttl), "IN", rrType, ntohs(cur->rdlength));
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

int sendQuery(char* hostToResolve, char* dns_server, struct sockaddr_in* server) {
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
    question->qtype = htons(MESSAGE_QTYPE_AAAA);

    fprintf(stderr, "Trying DNS server %s\n", dns_server);
    printDnsMessage(&test_message);
    sendDnsMessage(&test_message, server);
    printf("done sending... wait for reply!\n");
    return 0;
}

int getResponse(struct DNS_MESSAGE* response, struct sockaddr_in* server) {
    char* data = malloc(sizeof(char) * MAX_UDP_SIZE);
    socklen_t recvLen;
    
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
        //TODO: Should we exit here, or is where we re-initiate the loop with the next server?
        // This is where we "Return False" (1) and keep the loop going. Yay recursion.
        return 1;
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

int queryForNameAt(char* name, char* root_name) {
    if(strcmp(name, "0.0.0.0") == 0)//Don't do broadcast shouts!
        return 1;

    struct sockaddr_in server;
    int serverRet = 0;
    if (serverRet = initServer(&server, root_name) != 0) {
        printf("Invalid dns server.\n");
        return serverRet;
    }
    sendQuery(name, root_name, &server);

    struct DNS_MESSAGE response;
    getResponse(&response, &server);

    struct MESSAGE_HEADER_EXT ret;
    unpackExtendedMessageHeader(&(response.header), &ret);
    printDnsMessage(&response);

    // Start checking
    if (ret.description.qr_type == DNS_QR_TYPE_QUERY) {
    	fprintf(stderr, "Expected a response, received a query. Aborting.\n");
    	return RET_INVALID_RESPONSE;
    }

    //Jacob's Mess:
    uint32_t ip = 0u;
    struct MESSAGE_RESOURCE_RECORD *nxt;
    // We have a definitive answer, let's print stuff out
    if (ret.description.auth_answer == DNS_AA_TRUE) {
        printf("We got a definitive answer.\n");
        // do fun printing stuff
        return 0;//because it's a success.
    } else if (ret.additional_count > 0) {
        printf("We have addresses resolved.\n");
        // Let's hope that we got some IP addresses resolved for us.
        nxt = response.additional;

        if (nxt->type == MESSAGE_QTYPE_A) {
            memcpy(&ip, nxt->rd_data, sizeof(uint32_t));
            ip = ntohl(ip);
            printf(" autoIP: %d\n", ip);
        }
        //TODO: else if it's type is ... blah blah
    } else {
        printf("We need to resolve addresses.\n");
        //TODO: need to parse rdata of authority section
        //this requires forking a thread, or spawning a new process to resolve one of the names in the authority section. 
    }

    int count = 0;
    char next[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip, next, INET_ADDRSTRLEN);
    //IP might not hold a valid IP address right now. it could be 0u or if nxt->type != message_qtype_a...
    if(ret.answer_count == 0 && ret.nameserver_count > 0)//We've been delegated to another authority.
    {
    	//fprintf(stderr, "We should follow this redirect: %s \n", next);
    	fprintf(stderr, "Following a redirect\n");
    	//close(sockfd);
    	int answer = queryForNameAt(name, next);
    	while(answer != 0)
    	{
    	   nxt++;
    	   if(count < ret.nameserver_count)
    	   {
    	      getIPV4addr((uint32_t*)nxt->rd_data, next);
    	      answer = queryForNameAt(name, next);
    	   }
    	   else
    	   {
    	      return 1;
    	   }
    	}
    	if(answer == 0)
        {
            fprintf(stderr, "******WE GOT AN ANSWER!!*****3\nThis is the 'One layer up' catch\n");
            return 0;
        }
    }
    else if(ret.answer_count > 0)
    {
       fprintf(stderr,"****WE GOT AN ANSWER!!****2\n");
       return 0;
    }
    
    return 1;
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
    char *name = (char*)malloc(sizeof(char) * 1024);
    if (argc == 1) {
        name = "www.google.com"; //For now, simply use a hard-coded domain.
    } else if (argc == 2) {
        name = argv[1];
    } else {
        printf("Invalid argument.\n");
        return 1;
    }

    init();
    int ret = 1;

    while(ret != 0) {
        //Grabbed from A1 udp client
        char* root_name = getNextRootServer();
        if (root_name == NULL) {
            printf("No root server responded.\n");
            return 1;
        }

        ret = queryForNameAt(name, root_name);
    }
    close(sockfd);
    free(name);

    //testLabelSerdes(name);
}
