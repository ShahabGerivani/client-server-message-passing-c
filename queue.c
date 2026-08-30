#include <stdlib.h>
#include <string.h>
#include "queue.h"

int queue_init(struct queue *q, size_t initial_capacity, bool allow_growth) {
    if (initial_capacity == 0) {
        initial_capacity = 16; /* a reasonable default */
    }

    q->data = malloc(initial_capacity * sizeof(struct request));
    if (!q->data) {
        return -1;
    }

    q->capacity = initial_capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->allow_growth = allow_growth;
    return 0;
}

void queue_destroy(struct queue *q) {
    if (!q || !q->data) {
        return;
    }

    /* Free any remaining requests too, to avoid leaks */
    struct request tmp;
    while (queue_pop(q, &tmp)) {
        free(tmp.req_arg);
    }

    free(q->data);
    q->data = NULL;
    q->capacity = 0;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

/*
 * Doubles the capacity. Because the data is circular (head may be
 * greater than tail if it has wrapped around), we must preserve the
 * logical order (starting at head, for count elements) when copying
 * into the new array; a plain realloc would corrupt the ordering.
 */
static int queue_grow(struct queue *q) {
    size_t new_capacity = q->capacity * 2;
    struct request *new_data = malloc(new_capacity * sizeof(struct request));
    if (!new_data) {
        return -1;
    }

    for (size_t i = 0; i < q->count; i++) {
        new_data[i] = q->data[(q->head + i) % q->capacity];
    }

    free(q->data);
    q->data = new_data;
    q->capacity = new_capacity;
    q->head = 0;
    q->tail = q->count;
    return 0;
}

int queue_push(struct queue *q, const struct request *req) {
    if (queue_is_full(q)) {
        if (!q->allow_growth) {
            return -1; /* backpressure: caller should e.g. disable EPOLLIN */
        }
        if (queue_grow(q) != 0) {
            return -2;
        }
    }

    q->data[q->tail] = *req; /* shallow copy: client_fd, req_type,
                                 req_arg_len, and the req_arg pointer
                                 value are copied. Ownership of the
                                 buffer pointed to by req_arg is
                                 transferred to the queue from now on. */
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    return 0;
}

bool queue_pop(struct queue *q, struct request *out) {
    if (queue_is_empty(q)) {
        return false;
    }

    *out = q->data[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    return true;
}
