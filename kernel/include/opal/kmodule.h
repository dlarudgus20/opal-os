#ifndef OPAL_KMODULE_H
#define OPAL_KMODULE_H

#include <stddef.h>

#include <collections/linkedlist.h>

struct kmodule_desc;

struct kmodule {
    const struct kmodule_desc *desc;
    struct linkedlist_link link;
};

struct kmodule_desc {
    const char *name;
    void (*init)(void);
    void (*deinit)(void);
};

#define KMODULE_DECLARE(description, self) \
    static const struct kmodule_desc description; \
    static struct kmodule self = { .desc = &(description) }; \
    static struct kmodule *const g_kmodule_ptr__##self \
    [[maybe_unused, gnu::used, gnu::section(".kmodule")]] = &(self);

void kmodule_init(void);

#endif
