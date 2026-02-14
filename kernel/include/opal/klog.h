#ifndef OPAL_KLOG_H
#define OPAL_KLOG_H

#include <limits.h>
#include <stdint.h>

#include <kc/attributes.h>

#define KLOG_MAX_MSGLEN UINT16_MAX

enum { KLOG_NORMAL, KLOG_WRAP };

enum {
    KLOG_CRITICAL,
    KLOG_ERROR,
    KLOG_WARNING,
    KLOG_NOTICE,
    KLOG_INFO,
    KLOG_DEBUG,
    KLOG_LEVEL_COUNT,
};

struct klog_record_header {
    uint32_t seq;
    uint16_t msglen;
    uint16_t level;
};

void klog_init(void);
void klog_write(uint16_t level, const char *msg, uint16_t msglen);
bool klog_read(struct klog_record_header *header_out, char *msg_out, size_t msg_size);

int klog_format(uint16_t level, const char *fmt, const char *file, const char *func, unsigned line, ...) PRINTF_ATTR(2, 6);

#define klogf(level, fmt, ...) klog_format(level, fmt, __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#define kcritical(fmt, ...) klogf(KLOG_CRITICAL, fmt, ##__VA_ARGS__)
#define kerror(fmt, ...)    klogf(KLOG_ERROR, fmt, ##__VA_ARGS__)
#define kwarn(fmt, ...)     klogf(KLOG_WARNING, fmt, ##__VA_ARGS__)
#define knotice(fmt, ...)   klogf(KLOG_NOTICE, fmt, ##__VA_ARGS__)
#define kinfo(fmt, ...)     klogf(KLOG_INFO, fmt, ##__VA_ARGS__)
#define kdebug(fmt, ...)    klogf(KLOG_DEBUG, fmt, ##__VA_ARGS__)

#endif
