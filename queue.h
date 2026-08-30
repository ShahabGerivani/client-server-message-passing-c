#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>
#include <stdbool.h>
#include "request.h"

struct queue {
    struct request *data;   /* contiguous circular array */
    size_t capacity;        /* current capacity (number of slots) */
    size_t head;            /* index of the first element (for pop) */
    size_t tail;            /* index of the next free slot (for push) */
    size_t count;           /* number of elements currently stored */
    bool   allow_growth;    /* if true, the buffer resizes when full;
                                if false, push fails on a full queue
                                (hard backpressure) */
};

/*
 * Initializes the queue with the given initial capacity.
 * Returns 0 on success, -1 on error (e.g. malloc failure).
 */
int queue_init(struct queue *q, size_t initial_capacity, bool allow_growth);

/*
 * Frees the queue. Note: if elements with an allocated req_arg are
 * still present, this function frees them too, to avoid leaks.
 */
void queue_destroy(struct queue *q);

/*
 * Appends a request to the back of the queue (copies it, not a pointer).
 * Ownership of req->req_arg is transferred to the queue (i.e. from
 * this point on the queue is responsible for freeing it, unless the
 * push fails).
 *
 * Returns:
 *   0  : success
 *  -1  : queue was full and allow_growth=false (backpressure - the
 *        caller must handle this, e.g. by disabling EPOLLIN)
 *  -2  : memory error while growing
 */
int queue_push(struct queue *q, const struct request *req);

/*
 * Pops a request from the front of the queue into *out (copies it).
 * Ownership of req_arg is transferred to the caller; the caller must
 * free(out->req_arg) after using it.
 *
 * Returns true on success, false if the queue was empty.
 */
bool queue_pop(struct queue *q, struct request *out);

static inline bool queue_is_empty(const struct queue *q) {
    return q->count == 0;
}

static inline bool queue_is_full(const struct queue *q) {
    return q->count == q->capacity;
}

static inline size_t queue_size(const struct queue *q) {
    return q->count;
}

#endif
