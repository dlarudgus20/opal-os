#ifndef OPAL_PLATFORM_MM_DEFINES
#define OPAL_PLATFORM_MM_DEFINES

#define PAGE_SHIFT 12
#define PAGE_SIZE (1 << PAGE_SHIFT)

#define PTR_BIT_WIDTH 64
#define PFN_VALID_BIT_WIDTH (PTR_BIT_WIDTH - PAGE_SHIFT)

// docs/opal/memory-map.md

#define KERNEL_START_PHYS       0x00200000u
#define KERNEL_START_VIRT       0xffffffff80000000u
#define KSTACK_START_VIRT       0xffffffff8f000000u

#define DIRECT_MAP_START_VIRT   0xffff880000000000u
#define DIRECT_MAP_END_VIRT     0xffffc80000000000u
#define PAGES_START_VIRT        0xffffc90000000000u
#define PAGES_END_VIRT          0xffffca0000000000u
#define VMAP_START_VIRT       0xffffcb0000000000u
#define VMAP_END_VIRT         0xffffeb0000000000u

#ifdef OPAL_TEST
#define __kernel_start_lba  ((char*)1)
#define __rodata_end_lba    ((char*)1)
#define __before_stack_lba  ((char*)1)
#define __stack_bottom_lba  ((char*)1)
#define __kernel_end_lba    ((char*)1)
#else
// linker script
extern char __kernel_start_lba[];
extern char __rodata_end_lba[];
extern char __before_stack_lba[];
extern char __stack_bottom_lba[];
extern char __kernel_end_lba[];
#endif

#endif
