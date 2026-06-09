#include <kc/kassert.h>
#include <kc/stdlib.h>

#include <opal/kmodule.h>
#include <opal/klog.h>
#include <opal/mm/kmalloc.h>

#ifndef OPAL_TEST
extern struct kmodule *const kmodule_ptr_start__[];
extern struct kmodule *const kmodule_ptr_end__[];
#else
static struct kmodule *const kmodule_ptr_start__[1];
#define kmodule_ptr_end__ kmodule_ptr_start__
#endif

static struct linkedlist g_kmodules;

void kmodule_init(void) {
    linkedlist_init(&g_kmodules);

    for (struct kmodule *const *pptr = kmodule_ptr_start__; pptr < kmodule_ptr_end__; pptr++) {
        struct kmodule *const km = *pptr;

        linkedlist_push_back(&g_kmodules, &km->link);
        km->desc->init();

        kinfo("builtin kmodule loaded: %s", km->desc->name);
    }
}
