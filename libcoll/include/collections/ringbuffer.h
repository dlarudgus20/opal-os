#pragma once

#include <stddef.h>
#include <stdbool.h>

struct ringbuffer {
    char* buffer;
    size_t head;
    size_t tail;
    size_t count;
    size_t maxlen;
};

void ringbuffer_init(struct ringbuffer* rb, void* buffer, size_t maxlen);
bool ringbuffer_is_full(struct ringbuffer* rb);
bool ringbuffer_is_empty(struct ringbuffer* rb);

size_t ringbuffer_push_index(struct ringbuffer* rb);
size_t ringbuffer_pop_index(struct ringbuffer* rb);

#define ringbuffer_push(rb, type, val) (*(type*)((rb)->buffer + ringbuffer_push_index(rb) * sizeof(type)) = (val))

#define ringbuffer_pop(rb, type) (*(type*)((rb)->buffer + ringbuffer_pop_index(rb) * sizeof(type)))
