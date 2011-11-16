#include <stdio.h>
#include <stdio.h>
#include "myresolver.h"

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
    if (ret == NULL || header == NULL) {
        fprintf(stderr, "Uninitialized return struct.");
        return;
    }
    ret->id = header->id;
    ret->question_count = header->question_count;
    ret->answer_count = header->answer_count;
    ret->nameserver_count = header->nameserver_count;
    ret->additional_count = header->additional_count;

    ret->description.qr_type =          header->description & DNS_QR_MASK;
    ret->description.opcode =           header->description & DNS_OPCODE_MASK;
    ret->description.auth_answer =      header->description & DNS_AA_MASK;
    ret->description.trunc =            header->description & DNS_TRUNC_MASK;
    ret->description.recurse_desired =  header->description & DNS_RD_MASK;
    ret->description.recurse_available = header->description & DNS_RA_MASK;
    ret->description.Z =                header->description & DNS_Z_MASK;
    ret->description.resp_code =        header->description & DNS_RCODE_MASK;
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
