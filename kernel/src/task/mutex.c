#include <kc/kassert.h>

#include <opal/klog.h>
#include <opal/locks/irqlock.h>
#include <opal/task/mutex.h>
#include <opal/task/task.h>

static bool try_acquire(struct mutex *mutex) {
    struct task *current = task_current();

    if (mutex->owner == current) {
        panic("mutex: recursive lock");
    }

    if (mutex->owner) {
        return false;
    }

    mutex->owner = current;
    return true;
}

void mutex_init(struct mutex *mutex) {
    kassert(mutex);
    wait_list_init(&mutex->wait_list);
    mutex->owner = NULL;
}

void mutex_lock(struct mutex *mutex) {
    kassert(mutex);

    irqlock_t lock = irqlock_acquire();

    if (try_acquire(mutex)) {
        goto exit;
    }

    while (task_wait(&mutex->wait_list, TIMEOUT_INFINITY)) {
        if (try_acquire(mutex)) {
            break;
        }
    }

exit:
    irqlock_release(&lock);
}

void mutex_unlock(struct mutex *mutex) {
    kassert(mutex);

    irqlock_t lock = irqlock_acquire();

    kassert(mutex->owner == task_current(), "mutex: unlock by non-owner");
    mutex->owner = NULL;
    wait_list_wake_one(&mutex->wait_list);

    irqlock_release(&lock);
}
