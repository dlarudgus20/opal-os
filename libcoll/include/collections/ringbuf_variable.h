#pragma once

#include <stdbool.h>
#include <stddef.h>

/*
 * Variable-size record ring buffer.
 *
 * Record layout:
 *   [header bytes][size_t data_size][data bytes][alignment padding]
 *
 * header_size/header_align are fixed per ring instance.
 * used/maxsize are byte counters, not element counters.
 */
struct ringbuf_variable {
    char* buffer;
    size_t head;
    size_t tail;
    size_t used;
    size_t maxsize;
    size_t header_size;
    size_t header_align;
};

struct ringbuf_variable_data_view {
    const void* part1;
    size_t part1_size;
    const void* part2;
    size_t part2_size;
};

void ringbuf_variable_init(
    struct ringbuf_variable* rb,
    void* buffer,
    size_t maxsize,
    size_t header_size,
    size_t header_align
);

bool ringbuf_variable_is_full(struct ringbuf_variable* rb);
bool ringbuf_variable_is_empty(struct ringbuf_variable* rb);

bool ringbuf_variable_push(
    struct ringbuf_variable* rb,
    const void* header,
    const void* data,
    size_t data_size
);

bool ringbuf_variable_pop(
    struct ringbuf_variable* rb,
    void* header_out,
    /* Pointers reference internal ring memory and may be invalidated by later push operations. */
    struct ringbuf_variable_data_view* data_view_out
);
