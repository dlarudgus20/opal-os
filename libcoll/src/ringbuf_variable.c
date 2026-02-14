#include <stdint.h>

#include <kc/assert.h>
#include <kc/string.h>

#include "collections/ringbuf_variable.h"

static bool align_up(size_t value, size_t align, size_t* out) {
    size_t rem;
    size_t add;

    if (align == 0) {
        return false;
    }

    rem = value % align;
    add = rem == 0 ? 0 : (align - rem);
    if (value > SIZE_MAX - add) {
        return false;
    }

    *out = value + add;
    return true;
}

static bool compute_record_layout(
    size_t header_size,
    size_t header_align,
    size_t data_size,
    size_t* data_size_offset,
    size_t* data_offset,
    size_t* total_size
) {
    size_t size_off;
    size_t data_off;
    size_t raw;

    if (header_size == 0 || header_align == 0) {
        return false;
    }
    if (header_size > SIZE_MAX - sizeof(size_t)) {
        return false;
    }
    size_off = header_size;
    data_off = size_off + sizeof(size_t);
    if (data_off > SIZE_MAX - data_size) {
        return false;
    }
    raw = data_off + data_size;
    if (!align_up(raw, header_align, total_size)) {
        return false;
    }
    if (data_size_offset != NULL) {
        *data_size_offset = size_off;
    }
    if (data_offset != NULL) {
        *data_offset = data_off;
    }
    return true;
}

static size_t rb_advance(const struct ringbuf_variable* rb, size_t index, size_t delta) {
    size_t next = index + delta;
    if (next >= rb->maxsize) {
        next %= rb->maxsize;
    }
    return next;
}

static void rb_copy_out(const struct ringbuf_variable* rb, size_t index, void* dst, size_t size) {
    size_t first;
    size_t second;

    if (size == 0) {
        return;
    }

    first = rb->maxsize - index;
    if (first > size) {
        first = size;
    }
    memcpy(dst, rb->buffer + index, first);

    second = size - first;
    if (second != 0) {
        memcpy((char*)dst + first, rb->buffer, second);
    }
}

static void rb_copy_in(const struct ringbuf_variable* rb, size_t index, const void* src, size_t size) {
    size_t first;
    size_t second;

    if (size == 0) {
        return;
    }

    first = rb->maxsize - index;
    if (first > size) {
        first = size;
    }
    memcpy(rb->buffer + index, src, first);

    second = size - first;
    if (second != 0) {
        memcpy(rb->buffer, (const char*)src + first, second);
    }
}

void ringbuf_variable_init(
    struct ringbuf_variable* rb,
    void* buffer,
    size_t maxsize,
    size_t header_size,
    size_t header_align
) {
    assert(maxsize > 0, "ringbuf_variable maxsize must be greater than 0");
    assert(header_size > 0, "ringbuf_variable header_size must be greater than 0");
    assert(header_align > 0, "ringbuf_variable header_align must be greater than 0");

    rb->buffer = buffer;
    rb->head = 0;
    rb->tail = 0;
    rb->used = 0;
    rb->maxsize = maxsize;
    rb->header_size = header_size;
    rb->header_align = header_align;
}

bool ringbuf_variable_is_full(struct ringbuf_variable* rb) {
    return rb->used == rb->maxsize;
}

bool ringbuf_variable_is_empty(struct ringbuf_variable* rb) {
    return rb->used == 0;
}

bool ringbuf_variable_push(
    struct ringbuf_variable* rb,
    const void* header,
    const void* data,
    size_t data_size
) {
    size_t record_size;
    size_t free_total;
    size_t data_size_offset;
    size_t data_offset;
    size_t idx;

    if (header == NULL) {
        return false;
    }
    if (data_size == SIZE_MAX) {
        return false;
    }
    if (data_size != 0 && data == NULL) {
        return false;
    }
    if (!compute_record_layout(
            rb->header_size, rb->header_align, data_size, &data_size_offset, &data_offset, &record_size
        )) {
        return false;
    }

    free_total = rb->maxsize - rb->used;
    if (free_total < record_size) {
        return false;
    }

    idx = rb->tail;
    rb_copy_in(rb, idx, header, rb->header_size);
    idx = rb_advance(rb, idx, data_size_offset);

    rb_copy_in(rb, idx, &data_size, sizeof(size_t));
    idx = rb_advance(rb, idx, sizeof(size_t));
    assert(idx == rb_advance(rb, rb->tail, data_offset), "ringbuf_variable internal layout mismatch");

    if (data_size != 0) {
        rb_copy_in(rb, idx, data, data_size);
    }

    rb->tail = rb_advance(rb, rb->tail, record_size);
    rb->used += record_size;

    return true;
}

bool ringbuf_variable_pop(
    struct ringbuf_variable* rb,
    void* header_out,
    struct ringbuf_variable_data_view* data_view_out
) {
    size_t record_data_size;
    size_t record_size;
    size_t data_size_offset;
    size_t data_offset;
    size_t size_index;
    size_t data_index;
    size_t first_size;

    assert(rb->header_size > 0, "ringbuf_variable header_size must be greater than 0");
    assert(rb->header_align > 0, "ringbuf_variable header_align must be greater than 0");

    if (header_out == NULL) {
        return false;
    }
    if (rb->used == 0) {
        return false;
    }

    size_index = rb_advance(rb, rb->head, rb->header_size);
    rb_copy_out(rb, size_index, &record_data_size, sizeof(size_t));
    if (!compute_record_layout(
            rb->header_size, rb->header_align, record_data_size, &data_size_offset, &data_offset, &record_size
        )) {
        return false;
    }
    if (record_size > rb->used) {
        return false;
    }
    assert(data_size_offset == rb->header_size, "ringbuf_variable internal layout mismatch");

    rb_copy_out(rb, rb->head, header_out, rb->header_size);

    if (data_view_out != NULL) {
        data_index = rb_advance(rb, rb->head, data_offset);
        first_size = rb->maxsize - data_index;
        if (first_size > record_data_size) {
            first_size = record_data_size;
        }

        data_view_out->part1 = record_data_size == 0 ? NULL : rb->buffer + data_index;
        data_view_out->part1_size = first_size;
        data_view_out->part2 = (record_data_size > first_size) ? rb->buffer : NULL;
        data_view_out->part2_size = record_data_size - first_size;
    }

    rb->head = rb_advance(rb, rb->head, record_size);
    rb->used -= record_size;

    return true;
}
