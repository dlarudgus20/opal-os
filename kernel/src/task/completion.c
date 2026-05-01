#include <kc/kassert.h>
#include <kc/stdlib.h>

#include <opal/locks/irqlock.h>
#include <opal/task/completion.h>
#include <opal/task/task.h>

static bool try_acquire(struct completion *c) {
    return c->signaled;
}

void completion_init(struct completion *c) {
    kassert(c);
    wait_list_init(&c->wait_list);
    c->signaled = false;
}

void completion_signal(struct completion *c) {
    kassert(c);

    irqlock_t lock = irqlock_acquire();

    c->signaled = true;
    wait_list_wake_all(&c->wait_list);

    irqlock_release(&lock);
}

bool completion_wait(struct completion *c, uint64_t timeout) {
    kassert(c);

    irqlock_t lock = irqlock_acquire();
    bool ok = true;

    if (try_acquire(c)) {
        goto exit;
    }

    ok = task_wait(&c->wait_list, timeout);

exit:
    irqlock_release(&lock);
    return ok;
}
