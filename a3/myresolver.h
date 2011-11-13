/*
 * myresolver.h
 *
 *  Created on: Nov 13, 2011
 *      Author: myles
 */

#ifndef MYRESOLVER_H_
#define MYRESOLVER_H_

struct DNS_MESSAGE {
    MESSAGE_HEADER header;
    MESSAGE_QUESTION* question;
    MESSAGE_RESOURCE_RECORD* answer;
    MESSAGE_RESOURCE_RECORD* authority;
    MESSAGE_RESOURCE_RECORD* additional;
}__attribute__((__packed__));

struct MESSAGE_HEADER {
    U_INT16 id;
    U_INT16 description;
    U_INT16 question_count;
    U_INT16 answer_count;
    U_INT16 nameserver_count;
    U_INT16 additional_count;

}__attribute__((__packed__));

struct MESSAGE_QUESTION {
    char* qname; //A list of labels, must be parsed separately into struct.
    RR_TYPE qtype;
    QCLASS qclass;
}__attribute__((__packed__));

struct MESSAGE_RESOURCE_RECORD {
    char* name; //A list of labels, must be parsed into struct separately.
    RR_TYPE type;
    QCLASS class;
    U_INT32 ttl;
    U_INT16 rdlength;
    void* rd_data; // data format changes based on type.
}__attribute__((__packed__));

struct LABEL_LIST {
    U_INT8 length; //each dotted portion is a label, ie google.com is two labels and does not include the period.
    MESSAGE_LABEL* labels;
};

struct LABEL {
    U_INT8 length;
    char* message;
};

//QCLASS
typedef U_INT16 QCLASS;
const QCLASS MESSAGE_QCLASS_IN = 0x0001;

//TYPES
typedef U_INT16 RR_TYPE;
const RR_TYPE MESSAGE_QTYPE_A = 0x0001;
const RR_TYPE MESSAGE_QTYPE_NS = 0x0002;
const RR_TYPE MESSAGE_QTYPE_CNAME = 0x0005;
const RR_TYPE MESSAGE_QTYPE_STAR = 0x00FF;

//QR
const U_INT16 DNS_QR_MASK = 0x8000; //1000 0000 0000 0000
const U_INT16 DNS_QR_TYPE_QUERY = 0x8000; //1000 0000 0000 0000
const U_INT16 DNS_QR_TYPE_RESPONSE = 0x0000; //0000 0000 0000 0000

//opcode
const U_INT16 DNS_OPCODE_MASK = 0x7800; //0111 1000 0000 0000
const U_INT16 DNS_OPCODE_STANDARD_QUERY = 0x0000; // 0000 0000 0000 0000
const U_INT16 DNS_OPCODE_INVERSE_QUERY = 0x0800; //0000 1000 0000 0000
const U_INT16 DNS_OPCODE_SERVER_STATUS_REQ = 0x1000; // 0001 0000 0000 0000

//auth_answer
const U_INT16 DNS_AA_MASK = 0x0400; // 0000 0100 0000 0000
const U_INT16 DNS_AA_TRUE = 0x0400; // 0000 0100 0000 0000
const U_INT16 DNS_AA_FALSE = 0x0000; // 0000 0000 0000 0000

//truncated
const U_INT16 DNS_TRUNC_MASK = 0x0200; // 0000 0010 0000 0000
const U_INT16 DNS_TRUNC_TRUE = 0x0200; // 0000 0010 0000 0000
const U_INT16 DNS_TRUNC_FALSE = 0x0000; // 0000 0000 0000 0000

//recursion desired
const U_INT16 DNS_RD_MASK = 0x0100; // 0000 0001 0000 0000
const U_INT16 DNS_RD_TRUE = 0x0100; // 0000 0001 0000 0000
const U_INT16 DNS_RD_FALSE = 0x0000; // 0000 0000 0000 0000

//recursion available
const U_INT16 DNS_RA_MASK = 0x0080; // 0000 0000 1000 0000
const U_INT16 DNS_RA_TRUE = 0x0080; // 0000 0000 1000 0000
const U_INT16 DNS_RA_FALSE = 0x0000; // 0000 0000 0000 0000

//Z
const U_INT16 DNS_RA_MASK = 0X0070; // 0000 0000 0111 0000

//Response code
const U_INT16 DNS_RCODE_MASK = 0x000F; // 0000 0000 0000 1111
const U_INT16 DNS_RCODE_NOERROR = 0x0000; // 0000 0000 0000 0000
const U_INT16 DNS_RCODE_FORMAT_ERROR = 0x0001; // 0000 0000 0000 0001
const U_INT16 DNS_RCODE_SERVER_FAILURE = 0x0002; // 0000 0000 0000 0010
const U_INT16 DNS_RCODE_NAME_ERROR = 0x0003; // 0000 0000 0000 0011
const U_INT16 DNS_RCODE_NOT_IMPLEMENTED = 0x0004; // 0000 0000 0000 0100
const U_INT16 DNS_RCODE_REFUSED = 0x0005; // 0000 0000 0000 0101

struct QUERY_DESCRIPTION {
    U_INT16 qr_type; // bit 15
    U_INT16 opcode; // bit 14,13,12,11
    U_INT16 auth_answer; // bit 10
    U_INT16 trunc; // bit 9
    U_INT16 recurse_desired; // bit 8
    U_INT16 recurse_available; // bit 7
    U_INT16 Z; // bit 6, 5, 4
    U_INT16 resp_code; // 3,2,1,0
};

#endif /* MYRESOLVER_H_ */
