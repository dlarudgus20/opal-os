#include <kc/stdlib.h>

static void swap(void* a, void* b, size_t size) {
    for (char *ca = a, *cb = b; size-- > 0; ca++, cb++) {
        char tmp = *ca;
        *ca = *cb;
        *cb = tmp;
    }
}

void sort(void* ptr, size_t count, size_t size, int (*comp)(const void*, const void*)) {
    // heapsort
    char* p = ptr;
    size_t start = count / 2 * size; // first leaf
    size_t end = count * size;
    while (end > size) {
        if (start > 0) {
            start -= size;  // heapify
        } else {
            end -= size;    // pop
            swap(p + end, p, size);
        }

        // repair heap
        size_t root = start;
        while (1) {
            size_t child = root * 2 + size;
            if (child >= end) {
                break;
            }
            if (child + size < end && comp(p + child, p + child + size) < 0) {
                child += size;
            }

            if (comp(p + root, p + child) >= 0) {
                break;
            }
            swap(p + root, p + child, size);
            root = child;
        }
    }
}
