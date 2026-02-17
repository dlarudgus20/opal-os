#include <stddef.h>
#include <stdarg.h>
#include <stdalign.h>
#include <stdbool.h>

#include <kc/stdio.h>
#include <kc/stdlib.h>
#include <kc/string.h>
#include <kc/assert.h>

#include <opal/klog.h>

#define KLOG_WRAP UINT16_MAX
#define KLOG_BUFFER_SIZE 0x100000 // 1MB

struct klog_queue {
    char *buffer;
    uint32_t capacity;
    uint32_t write_pos;
    uint32_t write_seq;
    uint32_t read_pos;
    uint32_t drops;
    bool full;
};

static char klog_buffer[KLOG_BUFFER_SIZE];

static struct klog_queue klog_queue = {
    .buffer = klog_buffer,
    .capacity = sizeof(klog_buffer),
    .write_pos = 0,
    .write_seq = 0,
    .read_pos = 0,
    .drops = 0,
    .full = false,
};

static size_t get_record_size(size_t msglen) {
    return align_ceil_sz_p2(sizeof(struct klog_record_header) + msglen, sizeof(struct klog_record_header));
}

static uint32_t count_records(uint32_t from, uint32_t to) {
    uint32_t count = 0;
    uint32_t pos = from;
    uint32_t end = to > from ? to : to + klog_queue.capacity;
    while (pos < end) {
        if (pos >= klog_queue.capacity) {
            pos -= klog_queue.capacity;
            end -= klog_queue.capacity;
        }

        struct klog_record_header *header = (struct klog_record_header *)(klog_queue.buffer + pos);
        pos += get_record_size(header->msglen);
        count++;
    }
    return count;
}

void klog_init(void) {
}

void klog_write(uint16_t level, const char *msg, uint16_t msglen) {
    if (level >= KLOG_LEVEL_COUNT) {
        level = KLOG_LEVEL_COUNT - 1;
    }

    uint32_t record_size = get_record_size(msglen);

    uint32_t write_pos_1 = klog_queue.write_pos;
    uint32_t record_size_1 = record_size;
    uint16_t msglen_1 = msglen;
    uint16_t level_1 = level;

    uint16_t msglen_2 = 0;
    uint32_t record_size_2 = 0;
    uint32_t next_write_pos = write_pos_1 + record_size_1;
    bool dropped = false;

    if (write_pos_1 + record_size > klog_queue.capacity) {
        if (write_pos_1 + sizeof(struct klog_record_header) > klog_queue.capacity) {
            write_pos_1 = 0;
            next_write_pos = klog_queue.capacity;
        } else {
            msglen_1 = klog_queue.capacity - write_pos_1 - sizeof(struct klog_record_header);
            record_size_1 = sizeof(struct klog_record_header) + msglen_1;
            level_1 = KLOG_WRAP;

            msglen_2 = msglen - msglen_1;
            record_size_2 = get_record_size(msglen_2);
            next_write_pos = klog_queue.capacity + record_size_2;
        }
    }

    if (next_write_pos > klog_queue.read_pos + klog_queue.capacity) {
        klog_queue.drops += count_records(klog_queue.read_pos, next_write_pos - klog_queue.capacity);
        dropped = true;
    }

    struct klog_record_header header = {
        .seq = klog_queue.write_seq,
        .msglen = msglen_1,
        .level = level_1,
    };
    memcpy(klog_queue.buffer + write_pos_1, &header, sizeof(header));
    memcpy(klog_queue.buffer + write_pos_1 + sizeof(header), msg, msglen_1);
    klog_queue.write_seq += 1;
    klog_queue.write_pos += record_size_1;
    klog_queue.write_pos &= klog_queue.capacity - 1;

    if (level_1 == KLOG_WRAP) {
        header = (struct klog_record_header){
            .seq = klog_queue.write_seq,
            .msglen = msglen_2,
            .level = level,
        };
        memcpy(klog_queue.buffer, &header, sizeof(header));
        memcpy(klog_queue.buffer + sizeof(header), msg + msglen_1, msglen_2);
        klog_queue.write_seq += 1;
        klog_queue.write_pos = record_size_2;
    }

    if (dropped) {
        klog_queue.read_pos = klog_queue.write_pos;
    }

    if (klog_queue.write_pos == klog_queue.read_pos) {
        klog_queue.full = true;
    }
}

bool klog_read(struct klog_record_header *header_out, char *msg_out, size_t msg_size) {
    assert(header_out, "klog_read requires a non-NULL header_out");
    assert(msg_out && msg_size > 0, "klog_read requires a non-empty msg_out buffer");

    if (klog_queue.read_pos == klog_queue.write_pos && !klog_queue.full) {
        return false;
    }

    const char *const record_ptr = klog_queue.buffer + klog_queue.read_pos;
    memcpy(header_out, record_ptr, sizeof(*header_out));

    const uint16_t msglen = header_out->msglen;
    const size_t msglen_to_copy = msglen < msg_size - 1 ? msglen : msg_size - 1;
    memcpy(msg_out, record_ptr + sizeof(*header_out), msglen_to_copy);
    msg_out[msglen_to_copy] = '\0';

    klog_queue.read_pos += get_record_size(msglen);
    if (klog_queue.read_pos >= klog_queue.capacity) {
        klog_queue.read_pos -= klog_queue.capacity;
    }

    if (header_out->level == KLOG_WRAP) {
        klog_read(header_out, msg_out + msglen_to_copy, msg_size - msglen_to_copy);
    }

    klog_queue.full = false;

    return true;
}

int klog_format(uint16_t level, const char *fmt, const char *file, const char *func, unsigned line, ...) {
    char buf[KLOG_MAX_MSGLEN + 1];
    va_list args;
    va_start(args, line);

    int prefix = 0;
    int body = 0;

    prefix = snprintf_s(buf, sizeof(buf), "[%s:%s:%u] ", file, func, line);
    if (prefix < 0) {
        prefix = 0;
    }

    if ((size_t)prefix + 1 < sizeof(buf)) {
        body = vsnprintf_s(buf + prefix, sizeof(buf) - prefix, fmt, args);
    }

    if (body >= 0) {
        int written = prefix + body;
        if ((size_t)written + 1 >= sizeof(buf)) {
            written = sizeof(buf) - 1;
        }
        klog_write(level, buf, written);
    }

    va_end(args);
    return body >= 0 ? prefix + body : body;
}
