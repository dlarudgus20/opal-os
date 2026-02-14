#include <kc/assert.h>
#include "collections/ringbuffer.h"

void ringbuffer_init(struct ringbuffer* rb, void* buffer, size_t maxlen) {
    rb->buffer = buffer;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->maxlen = maxlen;
}

bool ringbuffer_is_full(struct ringbuffer* rb) {
    return rb->count == rb->maxlen;
}

bool ringbuffer_is_empty(struct ringbuffer* rb) {
    return rb->count == 0;
}

size_t ringbuffer_push_index(struct ringbuffer* rb) {
    assert(rb->count < rb->maxlen, "ringbuffer is full");

    size_t last = rb->tail;
    rb->tail += 1;
    if (rb->tail == rb->maxlen) {
        rb->tail = 0;
    }
    rb->count += 1;
    return last;
}

size_t ringbuffer_pop_index(struct ringbuffer* rb) {
    assert(rb->count > 0, "ringbuffer is empty");

    size_t first = rb->head;
    rb->head += 1;
    if (rb->head == rb->maxlen) {
        rb->head = 0;
    }
    rb->count -= 1;
    return first;
}
