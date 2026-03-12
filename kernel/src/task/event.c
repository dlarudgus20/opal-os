#include <kc/assert.h>
#include <kc/stdlib.h>

#include <opal/locks/irqlock.h>
#include <opal/task/event.h>
#include <opal/task/task.h>

static bool try_acquire(struct event *ev) {
    if (!ev->signaled) {
        return false;
    }

    if (ev->auto_reset) {
        ev->signaled = false;
    }

    return true;
}

void event_init(struct event *ev, bool auto_reset) {
    assert(ev);
    wait_list_init(&ev->wait_list);
    ev->signaled = false;
    ev->auto_reset = auto_reset;
}

void event_signal(struct event *ev) {
    assert(ev);

    irqlock_t lock = irqlock_acquire();

    if (ev->auto_reset) {
        ev->signaled = !wait_list_wake_one(&ev->wait_list);
    } else {
        ev->signaled = true;
        wait_list_wake_all(&ev->wait_list);
    }

    irqlock_release(&lock);
}

void event_reset(struct event *ev) {
    assert(ev);
    irqlock_t lock = irqlock_acquire();
    ev->signaled = false;
    irqlock_release(&lock);
}

bool event_wait(struct event *ev, uint64_t timeout) {
    assert(ev);

    irqlock_t lock = irqlock_acquire();
    bool ok = true;

    if (try_acquire(ev)) {
        goto exit;
    }

    ok = task_wait(&ev->wait_list, timeout);

exit:
    irqlock_release(&lock);
    return ok;
}
