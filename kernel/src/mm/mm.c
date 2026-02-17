#include <stdint.h>

#include <kc/string.h>

#include <opal/mm/mm.h>
#include <opal/mm/map.h>
#include <opal/mm/page.h>
#include <opal/platform/mm/pagetable.h>

void mm_init(void) {
    mm_map_init();

    const struct mmap* usable = mm_get_usable_map();
    struct mmap_entry snapshot_entries[MAX_USABLE_ENTRIES];
    struct mmap snapshot_mmap;

    memcpy(snapshot_entries, usable->entries, usable->length * sizeof(struct mmap_entry));
    snapshot_mmap.entries = snapshot_entries;
    snapshot_mmap.length = usable->length;

    mm_pagetable_init(&snapshot_mmap);
    mm_page_init(&snapshot_mmap);
}
