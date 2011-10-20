/*
 * awget.h
 *
 *  Created on: Oct 1, 2011
 *      Author: myles
 */

#ifndef AWGET_H_
#define AWGET_H_

struct ss_packet {
    uint8_t version;
    uint16_t ss_no;
    uint16_t url_len;
}__attribute__((__packed__));

// <<IP,Port>> Pair Sturcture
struct int_tuple {
    uint32_t ip_addr;
    uint32_t port_no;
}__attribute__((__packed__));

//Getting char IP
struct char_ip {
    char ch_ip[16];
}__attribute__((__packed__));

#endif /* AWGET_H_ */
