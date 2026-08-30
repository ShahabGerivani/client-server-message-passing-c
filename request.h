#ifndef REQUEST_H
#define REQUEST_H

#include <stdint.h>
#include <stdbool.h>

enum request_type {
    REQUEST_TYPE_LS = 0,
    REQUEST_TYPE_PWD = 1,
    REQUEST_TYPE_CAT = 2
};

struct request {
    int client_fd;           /* which client this request came from */
    enum request_type req_type;  /* only ever set via request_type_from_wire() */
    uint16_t req_arg_len;
    char *req_arg;           /* heap-allocated; owner is whoever last
                                 called queue_pop; must be freed after use */
};

/*
 * Converts a raw 2-byte REQ_TYPE value read from the wire (already
 * host-byte-order, i.e. after ntohs()) into a validated enum request_type.
 *
 * This MUST be used instead of casting the raw value directly, since a
 * malicious or buggy client could send a value outside the enum's range;
 * casting an out-of-range value to an enum type is not something you want
 * to rely on (its behavior is implementation-defined in C).
 *
 * Returns true and writes *out on success, false if raw_type does not
 * correspond to any known request_type (caller should reject the
 * request / respond with an error instead of queuing it).
 */
static inline bool request_type_from_wire(uint16_t raw_type, enum request_type *out) {
    switch (raw_type) {
        case REQUEST_TYPE_LS:
        case REQUEST_TYPE_PWD:
        case REQUEST_TYPE_CAT:
            *out = (enum request_type)raw_type;
            return true;
        default:
            return false;
    }
}

#endif
