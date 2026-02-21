#include <stdint.h>

#include <kc/string.h>

#include <opal/mm/mm.h>
#include <opal/mm/buddy.h>
#include <opal/mm/map.h>
#include <opal/mm/pfn.h>
#include <opal/platform/mm/pagetable.h>

void mm_init(void) {
    mm_map_init();

    mm_tmp_alloc_create();

    mm_pagetable_init();
    mm_pfn_init();

    mm_tmp_alloc_finalize();
    mm_buddy_init();
}
