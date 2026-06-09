#include <opal/kmodule.h>
#include <opal/klog.h>

KMODULE_DECLARE(g_desc, g_self)

static void init(void) {
    kinfo("[%s] Hello, world!", g_desc.name);
}

static const struct kmodule_desc g_desc = {
    .name = "hello",
    .init = init,
    .deinit = NULL,
};
