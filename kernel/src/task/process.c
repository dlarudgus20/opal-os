#include <kc/assert.h>
#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/task/task.h>
#include <opal/task/process.h>
#include <opal/task/elf.h>
#include <opal/locks/irqlock.h>
#include <opal/mm/mm.h>
#include <opal/mm/page.h>
#include <opal/mm/kmalloc.h>
#include <opal/fs/vfs.h>
#include <opal/platform/mm/pagetable.h>

#define MAX_REFC INT_MAX
#define PID_END INT_MAX

static int pid_compare(struct process *a, struct process *b) {
    return a->id - b->id;
}

static int pid_compare_to(struct process *a, pid_t b) {
    return a->id - b;
}

RBTREE_TEMPLATE(struct process, pid_t, pid_node, pid_compare, pid_compare_to, process, static)

static struct rbtree g_pid_tree;
static unsigned g_pid_next;

void proc_tree_init(void) {
    g_pid_next = 0;
    rbtree_init(&g_pid_tree);
}

void process_init(struct process *proc, struct pagetable *ptbl) {
    assert(g_pid_next < PID_END);

    proc->id = g_pid_next++;
    proc->refcount = 1;
    proc->pagetable = ptbl;

    linkedlist_init(&proc->task_list);
    vmtree_init(&proc->vmtree);
    dynarray_init(&proc->open_files);
    rbtree_insert_process(&g_pid_tree, proc);
}

procptr_t process_create(void) {
    struct pagetable *ptbl = pagetable_create();
    if (!ptbl) {
        return (procptr_t){ .ptr = NULL };
    }

    struct process *proc = kzalloc(sizeof(*proc));
    if (!proc) {
        pagetable_destroy(ptbl);
        return (procptr_t){ .ptr = NULL };
    }

    irqlock_t irqlock = irqlock_acquire();
    process_init(proc, ptbl);
    irqlock_release(&irqlock);
    return (procptr_t){ .ptr = proc };
}

static void free_mapped_pages_all(struct process *proc) {
    struct vmtree_iter iter = vmtree_before_begin(&proc->vmtree);
    struct vmtree_entry entry;
    while (vmtree_iter_next(&iter, &entry)) {
        for (uintptr_t va = entry.start; va < entry.end; va += PAGE_SIZE) {
            phys_addr_t pa;
            if (!pagetable_lookup(proc->pagetable, va, &pa)) {
                continue;
            }
            mm_free_page(phys_to_pfn(pa & PTE_MASK_ADDR), 0);
        }
    }
}

static void destroy_open_files(struct process *proc) {
    dynarray_foreach(struct file **, pfile, &proc->open_files) {
        if (*pfile) {
            file_release(*pfile);
        }
    }
    dynarray_destroy(&proc->open_files);
}

static void process_free(struct process *proc) {
    if (!linkedlist_is_empty(&proc->task_list)) {
        panic("cannot destroy process with active tasks");
    }
    destroy_open_files(proc);
    rbtree_remove(&g_pid_tree, &proc->pid_node);
    free_mapped_pages_all(proc);
    pagetable_destroy(proc->pagetable);
    vmtree_destroy(&proc->vmtree);
    kfree(proc, sizeof(*proc));
}

struct process *process_current(void) {
    return task_current()->process.ptr;
}

procptr_t process_from_id(pid_t id) {
    irqlock_t irqlock = irqlock_acquire();

    struct rbtree_find_result result = rbtree_find_process(&g_pid_tree, id);
    if (result.lower == NULL || result.lower != result.upper) {
        irqlock_release(&irqlock);
        return (procptr_t){ .ptr = NULL };
    }

    struct process *proc = container_of(result.lower, struct process, pid_node);
    assert(proc->refcount < MAX_REFC);
    proc->refcount++;

    irqlock_release(&irqlock);
    return (procptr_t){ .ptr = proc };
}

procptr_t process_retain(struct process *proc) {
    irqlock_t irqlock = irqlock_acquire();
    assert(proc->refcount < MAX_REFC);
    proc->refcount++;
    irqlock_release(&irqlock);
    return (procptr_t){ .ptr = proc };
}

pid_t process_release(procptr_t proc) {
    irqlock_t irqlock = irqlock_acquire();

    assert(proc.ptr->refcount > 0);
    proc.ptr->refcount--;

    pid_t id = proc.ptr->id;

    if (proc.ptr->refcount == 0) {
        process_free(proc.ptr);
        id = PID_INVALID;
    }

    irqlock_release(&irqlock);
    return id;
}

fd_t process_open_file(struct process *proc, struct file *file) {
    irqlock_t irqlock = irqlock_acquire();

    size_t len = dynarray_len(&proc->open_files, sizeof(struct file *));
    if (len >= FD_MAX) {
        goto err;
    }

    struct file **pfile = dynarray_push_back(&proc->open_files, sizeof(*pfile));
    if (!pfile) {
        goto err;
    }

    file_retain(file);
    *pfile = file;

    irqlock_release(&irqlock);
    return (fd_t)len;

err:
    irqlock_release(&irqlock);
    return FD_INVALID;
}

struct file *process_get_file(struct process *proc, fd_t fd) {
    irqlock_t irqlock = irqlock_acquire();

    size_t len = dynarray_len(&proc->open_files, sizeof(struct file *));
    if (fd < 0 || (size_t)fd >= len) {
        irqlock_release(&irqlock);
        return NULL;
    }

    struct file *file = dynarray_at(&proc->open_files, struct file *, (size_t)fd);
    if (file) {
        file_retain(file);
    }
    irqlock_release(&irqlock);
    return file;
}

bool process_close_file(struct process *proc, fd_t fd) {
    irqlock_t irqlock = irqlock_acquire();

    size_t len = dynarray_len(&proc->open_files, sizeof(struct file *));
    if (fd < 0 || (size_t)fd >= len) {
        goto err;
    }

    struct file **pfile = &dynarray_at(&proc->open_files, struct file *, (size_t)fd);
    if (!*pfile) {
        goto err;
    }

    file_release(*pfile);
    *pfile = NULL;

    irqlock_release(&irqlock);
    return true;

err:
    irqlock_release(&irqlock);
    return false;
}

static void free_mapped_range(struct process *proc, virt_addr_t addr, virt_size_t mapped_len) {
    for (virt_size_t off = 0; off < mapped_len; off += PAGE_SIZE) {
        phys_addr_t pa;
        if (!pagetable_lookup(proc->pagetable, addr + off, &pa)) {
            continue;
        }
        mm_free_page(phys_to_pfn(pa & PTE_MASK_ADDR), 0);
    }
}

static enum pload_status map_section(struct process *proc, void *data, size_t filesz, virt_addr_t addr, virt_size_t memsz, uint32_t flags) {
    irqlock_t irqlock = irqlock_acquire();
    enum pload_status result = PLOAD_NOMEM;

    vmtree_status_t insertion = vmtree_insert(&proc->vmtree, addr, addr + memsz, proc);
    if (insertion != VMTREE_OK) {
        if (insertion == VMTREE_ERR_EXISTS) {
            result = PLOAD_BAD_IMAGE;
            goto err;
        }
        goto err;
    }

    uint8_t order = 0;
    for (; memsz >= ((size_t)PAGE_SIZE << (order + 1)); order++) {}

    unsigned char *bytes = data;
    size_t filesz_remain = filesz;
    virt_addr_t current_addr = addr;
    size_t memsz_remain = memsz;
    while (memsz_remain > 0) {
        size_t size = (size_t)PAGE_SIZE << order;
        if (order > 0 && size > memsz_remain) {
            order--;
            continue;
        }

        pfn_t pfn = mm_alloc_page(order);
        if (pfn == PFN_INVALID) {
            if (order == 0) {
                goto err_alloc;
            }
            order--;
            continue;
        }

        page_entry_t ptbl_flags = PTBL_USER;
        if (flags & ELF_PF_W) {
            ptbl_flags |= PTBL_WRITABLE;
        }

        virt_addr_t map_rs = pagetable_map(proc->pagetable, current_addr, pfn_to_phys(pfn), size, ptbl_flags);
        if (map_rs == 0) {
            mm_free_page(pfn, order);
            goto err_alloc;
        }

        current_addr += size;
        if (memsz_remain < size) {
            memsz_remain = 0;
        } else {
            memsz_remain -= size;
        }

        unsigned char *ptr = pfn_to_direct_ptr(pfn);
        if (filesz_remain > 0) {
            size_t to_copy = filesz_remain;
            if (to_copy > size) {
                to_copy = size;
            }
            memcpy(ptr, bytes, to_copy);
            filesz_remain -= to_copy;
            size -= to_copy;
            bytes += to_copy;
            ptr += to_copy;
        }
        memset(ptr, 0, size);
    }

    irqlock_release(&irqlock);
    return PLOAD_OK;

err_alloc:
    free_mapped_range(proc, addr, current_addr - addr);
    pagetable_unmap(proc->pagetable, addr, current_addr - addr, false);
    vmtree_remove(&proc->vmtree, addr, addr + memsz);
err:
    irqlock_release(&irqlock);
    return result;
}

static void process_entry(void) {
    virt_addr_t *kstack = task_current()->kstack;
    enter_userland(kstack[0], kstack[1]);
}

taskptr_t process_create_usertask(
    struct process *proc, virt_addr_t entry, virt_addr_t stack, virt_size_t stack_size, enum task_priority priority
) {
    taskptr_t task = task_create(proc, process_entry, priority);
    if (!task.ptr) {
        return task;
    }

    virt_addr_t *kstack = task.ptr->kstack;
    kstack[0] = entry;
    kstack[1] = stack + stack_size;
    task_resume(task.ptr);

    return task;
}

enum pload_status process_load_elf(struct process *proc, void *elf, size_t size, taskptr_t *out) {
    if (size < sizeof(struct elf64_header)) {
        return PLOAD_BAD_IMAGE;
    }

    struct elf64_header *hdr = elf;
    struct elf_ident ident = {
        .magic = { '\x7f', 'E', 'L', 'F' },
        .elf_class = ELF_CLASS64,
        .encoding = ELF_DATA2LSB,
        .elf_version = ELF_VERSION,
        .os_abi = ELF_OSABI_NONE,
    };
    if (memcmp(&hdr->ident, &ident, sizeof(ident)) != 0) {
        return PLOAD_BAD_IMAGE;
    }

    if (hdr->version != ELF_VERSION) {
        return PLOAD_BAD_IMAGE;
    }
    if (hdr->eh_size < sizeof(*hdr)) {
        return PLOAD_BAD_IMAGE;
    }

    if (hdr->type != ELF_ET_EXEC) {
        return PLOAD_NO_EXEC;
    }
    if (hdr->machine != ELF_X86_64) {
        return PLOAD_NO_EXEC;
    }

    if (hdr->ph_ent_size < sizeof(struct elf64_phdr)) {
        return PLOAD_BAD_IMAGE;
    }
    if (hdr->ph_off >= size) {
        return PLOAD_BAD_IMAGE;
    }
    if (hdr->ph_num > (size - hdr->ph_off) / hdr->ph_ent_size) {
        return PLOAD_BAD_IMAGE;
    }
    if (hdr->ph_ent_size % alignof(struct elf64_phdr) != 0) {
        return PLOAD_BAD_IMAGE;
    }

    unsigned char *bytes = elf;
    virt_addr_t ustack_bottom = 0x0000400000000000;

    if (hdr->entry >= ustack_bottom) {
        return PLOAD_NO_EXEC;
    }

    for (uint32_t i = 0; i < hdr->ph_num; i++) {
        struct elf64_phdr *phdr = (struct elf64_phdr *)(bytes + hdr->ph_off + hdr->ph_ent_size * i);

        switch (phdr->type) {
            case ELF_PT_NULL:
            case ELF_PT_NOTE:
                continue;
            case ELF_PT_LOAD:
                break;
            case ELF_PT_DYNAMIC:
            case ELF_PT_INTERP:
            case ELF_PT_SHLIB:
            case ELF_PT_TLS:
            default:
                return PLOAD_NO_EXEC;
        }

        if (phdr->filesz > phdr->memsz) {
            return PLOAD_BAD_IMAGE;
        }
        if (phdr->offset > size) {
            return PLOAD_BAD_IMAGE;
        }
        if (phdr->filesz > size - phdr->offset) {
            return PLOAD_BAD_IMAGE;
        }
        if (phdr->vaddr + phdr->memsz < phdr->vaddr) {
            return PLOAD_BAD_IMAGE;
        }
        if (phdr->align != 0 && !ispower2(phdr->align)) {
            return PLOAD_BAD_IMAGE;
        }
        if (phdr->align > 1 && (phdr->vaddr % phdr->align != phdr->offset % phdr->align)) {
            return PLOAD_BAD_IMAGE;
        }
        if (phdr->vaddr % PAGE_SIZE != 0 || phdr->offset % PAGE_SIZE != 0) {
            return PLOAD_NO_EXEC;
        }
        if (phdr->vaddr + phdr->memsz >= ustack_bottom) {
            return PLOAD_NO_EXEC;
        }

        enum pload_status result = map_section(proc, bytes + phdr->offset, phdr->filesz, phdr->vaddr, phdr->memsz, phdr->flags);
        if (result != PLOAD_OK) {
            return result;
        }
    }

    enum pload_status result =  map_section(proc, NULL, 0, ustack_bottom, PAGE_SIZE, ELF_PF_R | ELF_PF_W);
    if (result != PLOAD_OK) {
        return result;
    }

    *out = process_create_usertask(proc, hdr->entry, ustack_bottom, PAGE_SIZE, TASK_PRIORITY_NORMAL);
    if (!out->ptr) {
        return PLOAD_NOMEM;
    }

    return PLOAD_OK;
}
