#ifndef MYRESOLVER_H_
#define MYRESOLVER_H_

#include <stdlib.h>
#include <stdint.h>

typedef uint16_t RR_TYPE;
typedef uint16_t QCLASS;

// For DNSSEC 4096 is the recommended max size.
const uint16_t MAX_UDP_SIZE = 4096;
const uint16_t DNS_PORT = 53;

const int RET_FOUND_ANSWER = 0;
const int RET_INVALID_IP = 1;
const int RET_INVALID_RESPONSE = 2;
const int RET_ANSWER_NOT_FOUND = 3;
const int RET_ABORT = -1;
const int RET_NO_SUCH_NAME = -2;

//QCLASS
const QCLASS MESSAGE_QCLASS_IN = 0x0001;

//TYPES
const RR_TYPE MESSAGE_QTYPE_A = 0x0001;
const RR_TYPE MESSAGE_QTYPE_NS = 0x0002;
const RR_TYPE MESSAGE_QTYPE_CNAME = 0x0005;
const RR_TYPE MESSAGE_QTYPE_SOA = 0x0006;
const RR_TYPE MESSAGE_QTYPE_STAR = 0x00FF;
const RR_TYPE MESSAGE_QTYPE_AAAA = 0x001C;
const RR_TYPE MESSAGE_QTYPE_RRSIG = 0x002E;

//QR
const uint16_t DNS_QR_MASK = 0x8000; //1000 0000 0000 0000
const uint16_t DNS_QR_TYPE_QUERY = 0x0000; //1000 0000 0000 0000
const uint16_t DNS_QR_TYPE_RESPONSE = 0x8000; //0000 0000 0000 0000

//opcode
const uint16_t DNS_OPCODE_MASK = 0x7800; //0111 1000 0000 0000
const uint16_t DNS_OPCODE_STANDARD_QUERY = 0x0000; // 0000 0000 0000 0000
const uint16_t DNS_OPCODE_INVERSE_QUERY = 0x0800; //0000 1000 0000 0000
const uint16_t DNS_OPCODE_SERVER_STATUS_REQ = 0x1000; // 0001 0000 0000 0000

//auth_answer
const uint16_t DNS_AA_MASK = 0x0400; // 0000 0100 0000 0000
const uint16_t DNS_AA_TRUE = 0x0400; // 0000 0100 0000 0000
const uint16_t DNS_AA_FALSE = 0x0000; // 0000 0000 0000 0000

//truncated
const uint16_t DNS_TRUNC_MASK = 0x0200; // 0000 0010 0000 0000
const uint16_t DNS_TRUNC_TRUE = 0x0200; // 0000 0010 0000 0000
const uint16_t DNS_TRUNC_FALSE = 0x0000; // 0000 0000 0000 0000

//recursion desired
const uint16_t DNS_RD_MASK = 0x0100; // 0000 0001 0000 0000
const uint16_t DNS_RD_TRUE = 0x0100; // 0000 0001 0000 0000
const uint16_t DNS_RD_FALSE = 0x0000; // 0000 0000 0000 0000

//recursion available
const uint16_t DNS_RA_MASK = 0x0080; // 0000 0000 1000 0000
const uint16_t DNS_RA_TRUE = 0x0080; // 0000 0000 1000 0000
const uint16_t DNS_RA_FALSE = 0x0000; // 0000 0000 0000 0000

//Z
const uint16_t DNS_Z_MASK = 0x0070; // 0000 0000 0111 0000

//Response code
const uint16_t DNS_RCODE_MASK = 0x000F; // 0000 0000 0000 1111
const uint16_t DNS_RCODE_NOERROR = 0x0000; // 0000 0000 0000 0000
const uint16_t DNS_RCODE_FORMAT_ERROR = 0x0001; // 0000 0000 0000 0001
const uint16_t DNS_RCODE_SERVER_FAILURE = 0x0002; // 0000 0000 0000 0010
const uint16_t DNS_RCODE_NAME_ERROR = 0x0003; // 0000 0000 0000 0011
const uint16_t DNS_RCODE_NOT_IMPLEMENTED = 0x0004; // 0000 0000 0000 0100
const uint16_t DNS_RCODE_REFUSED = 0x0005; // 0000 0000 0000 0101

struct MESSAGE_HEADER {
    uint16_t id;
    uint16_t description;
    uint16_t question_count;
    uint16_t answer_count;
    uint16_t nameserver_count;
    uint16_t additional_count;

}__attribute__((__packed__));

struct MESSAGE_QUESTION {
    uint8_t* qname; //A list of labels, must be parsed separately into struct.
    RR_TYPE qtype;
    QCLASS qclass;
}__attribute__((__packed__));

struct MESSAGE_RESOURCE_RECORD {
    uint8_t* name; //A list of labels, must be parsed into struct separately.
    RR_TYPE type;
    QCLASS class;
    uint32_t ttl;
    uint16_t rdlength;
    void* rd_data; // data format changes based on type.
}__attribute__((__packed__));

struct DNS_MESSAGE {
    struct MESSAGE_HEADER header;
    struct MESSAGE_QUESTION* question;
    struct MESSAGE_RESOURCE_RECORD* answer;
    struct MESSAGE_RESOURCE_RECORD* authority;
    struct MESSAGE_RESOURCE_RECORD* additional;
}__attribute__((__packed__));

struct LABEL_LIST {
    uint8_t length; //each dotted portion is a label, ie google.com is two labels and does not include the period.
    struct LABEL* labels;
};

struct LABEL {
    uint8_t length;
    char* message;
};

struct MESSAGE_DESCRIPTION {
    uint16_t qr_type; // bit 15
    uint16_t opcode; // bit 14,13,12,11
    uint16_t auth_answer; // bit 10
    uint16_t trunc; // bit 9
    uint16_t recurse_desired; // bit 8
    uint16_t recurse_available; // bit 7
    uint16_t Z; // bit 6, 5, 4
    uint16_t resp_code; // 3,2,1,0
}__attribute__((__packed__));

struct MESSAGE_HEADER_EXT {
    uint16_t id;
    struct MESSAGE_DESCRIPTION description;
    uint16_t question_count;
    uint16_t answer_count;
    uint16_t nameserver_count;
    uint16_t additional_count;
};

struct RR_CNAME {
	uint8_t* name;
}__attribute__((__packed__));

struct RR_NS {
	uint8_t* name;
}__attribute__((__packed__));

struct RR_SOA {
	// Not sure what this is
	uint8_t* mname;
	// Not sure what this is
	uint8_t* rname;
	uint32_t serial;
	uint32_t refresh_interval;
	uint32_t retry_interval;
	uint32_t expire_interval;
	uint32_t minimum_ttl;
}__attribute__((__packed__));

struct RR_A {
	uint32_t ip;
}__attribute__((__packed__));

struct RR_AAAA {
	uint8_t ip[16];
}__attribute__((__packed__));

struct RR_SIG {
	uint16_t type_covered;
	uint8_t algorithm;
	uint8_t labels;
	uint32_t ttl;
	uint32_t sig_expiration;
	uint32_t sig_inception;
	uint16_t key_tag;
	// Must not be compressed, must not have C0?
	uint8_t* signer_name;
	// Calculate length by getting rdlength and subtracting everything else
	char* signature;
}__attribute__((__packed__));

struct LABEL_LIST* parseLabels(char* str);
int loopThroughList(char* name, char** list, int count, RR_TYPE query_type, struct MESSAGE_RESOURCE_RECORD** answer);
int loopThroughRRs(char* name, char* last_root, struct MESSAGE_RESOURCE_RECORD* answers, uint16_t count, RR_TYPE query_type, struct MESSAGE_RESOURCE_RECORD** answer);
int queryForNameAt(char* name, char* root_name, RR_TYPE query_type, struct MESSAGE_RESOURCE_RECORD** answer);
void unpackExtendedMessageHeader(struct MESSAGE_HEADER* header, struct MESSAGE_HEADER_EXT* ret);
void repackExtendedMessageHeader(struct MESSAGE_HEADER_EXT* header, struct MESSAGE_HEADER* ret);

#endif /* MYRESOLVER_H_ */

