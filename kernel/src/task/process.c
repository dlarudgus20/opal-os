#include <limits.h>

#include <kc/kassert.h>
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
#include <opal/platform/interrupt.h>
#include <opal/platform/mm/pagetable.h>
#include <opal/platform/task/context.h>

#define MAX_REFC INT_MAX
#define PID_END INT_MAX

static int pid_compare(struct process *a, struct process *b) {
    return a->id - b->id;
}

static int pid_compare_to(struct process *a, pid_t b) {
    return a->id - b;
}

RBTREE_TEMPLATE(struct process, pid_t, pid_node, pid_compare, pid_compare_to, process, static)

struct vm_area {
    page_entry_t ptbl_flags;
};

struct process_image {
    struct pagetable *pagetable;
    struct vmtree vmtree;
};

static void free_mapped_range(struct pagetable *ptbl, virt_addr_t addr, virt_size_t mapped_len);

static struct rbtree g_pid_tree;
static unsigned g_pid_next;

void proc_tree_init(void) {
    g_pid_next = 0;
    rbtree_init(&g_pid_tree);
}

void process_init(struct process *proc, struct pagetable *ptbl) {
    kassert(g_pid_next < PID_END);

    proc->id = g_pid_next++;
    proc->refcount = 1;
    proc->pagetable = ptbl;

    linkedlist_init(&proc->task_list);
    vmtree_init(&proc->vmtree);
    completion_init(&proc->exit_compl);
    filetable_init(&proc->open_files);

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

static void process_image_init(struct process_image *image, struct pagetable *ptbl) {
    image->pagetable = ptbl;
    vmtree_init(&image->vmtree);
}

static kerrno_t process_image_create(struct process_image *image) {
    struct pagetable *ptbl = pagetable_create();
    if (!ptbl) {
        return OPAL_ENOMEM;
    }

    process_image_init(image, ptbl);
    return OPAL_OK;
}

static void free_mapped_pages_all(struct pagetable *ptbl, struct vmtree *vmtree) {
    struct vmtree_iter iter = vmtree_before_begin(vmtree);
    struct vmtree_entry entry;
    while (vmtree_iter_next(&iter, &entry)) {
        for (uintptr_t va = entry.start; va < entry.end; va += PAGE_SIZE) {
            phys_addr_t pa;
            if (!pagetable_lookup(ptbl, va, &pa)) {
                continue;
            }
            mm_free_page(phys_to_pfn(pa & PTE_MASK_ADDR), 0);
        }
    }
}

static void free_vm_areas_all(struct vmtree *vmtree) {
    struct vmtree_iter iter = vmtree_before_begin(vmtree);
    struct vmtree_entry entry;
    while (vmtree_iter_next(&iter, &entry)) {
        kfree(entry.entry, sizeof(struct vm_area));
    }
}

static void process_image_destroy_parts(struct pagetable *ptbl, struct vmtree *vmtree) {
    free_mapped_pages_all(ptbl, vmtree);
    free_vm_areas_all(vmtree);
    pagetable_destroy(ptbl);
    vmtree_destroy(vmtree);
}

static void process_image_destroy(struct process_image *image) {
    process_image_destroy_parts(image->pagetable, &image->vmtree);
}

static void process_free(struct process *proc) {
    if (!linkedlist_is_empty(&proc->task_list)) {
        panic("cannot destroy process with active tasks");
    }
    filetable_destroy(&proc->open_files);
    rbtree_remove(&g_pid_tree, &proc->pid_node);
    process_image_destroy_parts(proc->pagetable, &proc->vmtree);
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
    kassert(proc->refcount < MAX_REFC);
    proc->refcount++;

    irqlock_release(&irqlock);
    return (procptr_t){ .ptr = proc };
}

procptr_t process_retain(struct process *proc) {
    irqlock_t irqlock = irqlock_acquire();
    kassert(proc->refcount < MAX_REFC);
    proc->refcount++;
    irqlock_release(&irqlock);
    return (procptr_t){ .ptr = proc };
}

pid_t process_release(procptr_t proc) {
    irqlock_t irqlock = irqlock_acquire();

    kassert(proc.ptr->refcount > 0);
    proc.ptr->refcount--;

    pid_t id = proc.ptr->id;

    if (proc.ptr->refcount == 0) {
        process_free(proc.ptr);
        id = PID_INVALID;
    }

    irqlock_release(&irqlock);
    return id;
}

bool process_join(struct process *proc, uint64_t timeout) {
    return completion_wait(&proc->exit_compl, timeout);
}

fd_t process_open_file(struct process *proc, fd_t fd, struct file *file) {
    irqlock_t irqlock = irqlock_acquire();
    if (fd == FD_INVALID) {
        fd = filetable_insert(&proc->open_files, file);
    } else {
        if (!kerrno_ok(filetable_insert_at(&proc->open_files, fd, file))) {
            fd = FD_INVALID;
        }
    }
    irqlock_release(&irqlock);
    return fd;
}

struct file *process_get_file(struct process *proc, fd_t fd) {
    irqlock_t irqlock = irqlock_acquire();
    struct file *file = filetable_get(&proc->open_files, fd);
    irqlock_release(&irqlock);
    return file;
}

bool process_close_file(struct process *proc, fd_t fd) {
    irqlock_t irqlock = irqlock_acquire();
    bool ok = filetable_remove(&proc->open_files, fd);
    irqlock_release(&irqlock);
    return ok;
}

fd_t process_dup_fd(struct process *proc, fd_t oldfd, fd_t newfd) {
    irqlock_t irqlock = irqlock_acquire();
    fd_t ret = FD_INVALID;

    struct file *file = filetable_get(&proc->open_files, oldfd);
    if (!file) {
        goto ret;
    }

    if (newfd == oldfd) {
        ret = newfd;
    } else if (newfd == FD_INVALID) {
        ret = filetable_insert(&proc->open_files, file);
    } else if (kerrno_ok(filetable_insert_at(&proc->open_files, newfd, file))) {
        ret = newfd;
    }
    file_release(file);

ret:
    irqlock_release(&irqlock);
    return ret;
}

static bool process_has_current_task_only(struct process *proc) {
    irqlock_t irqlock = irqlock_acquire();

    struct task *current = task_current();
    struct linkedlist_link *head = linkedlist_head(&proc->task_list);
    bool result = !linkedlist_is_nil(&proc->task_list, head) && head == &current->proc_link
        && head->next == linkedlist_nil(&proc->task_list);

    irqlock_release(&irqlock);
    return result;
}

static kerrno_t clone_vmem_range(
    struct process *child, struct process *parent, struct vmtree_entry entry) {
    struct vm_area *src_area = entry.entry;
    struct vm_area *dst_area = kzalloc(sizeof(*dst_area));
    if (!dst_area) {
        return OPAL_ENOMEM;
    }

    *dst_area = *src_area;

    vmtree_status_t insertion = vmtree_insert(&child->vmtree, entry.start, entry.end, dst_area);
    if (insertion != VMTREE_OK) {
        kfree(dst_area, sizeof(*dst_area));
        return insertion == VMTREE_ERR_EXISTS ? OPAL_EEXIST : OPAL_ENOMEM;
    }

    kerrno_t result = OPAL_OK;
    virt_addr_t current_addr = entry.start;
    for (; current_addr < entry.end; current_addr += PAGE_SIZE) {
        phys_addr_t src_pa;
        if (!pagetable_lookup(parent->pagetable, current_addr, &src_pa)) {
            result = OPAL_EINVAL;
            goto err;
        }

        pfn_t dst_pfn = mm_alloc_page(0);
        if (dst_pfn == PFN_INVALID) {
            result = OPAL_ENOMEM;
            goto err;
        }

        memcpy(pfn_to_direct_ptr(dst_pfn), phys_to_direct_ptr(src_pa & PTE_MASK_ADDR), PAGE_SIZE);

        if (pagetable_map(child->pagetable, current_addr, pfn_to_phys(dst_pfn), PAGE_SIZE,
                dst_area->ptbl_flags)
            == 0) {
            mm_free_page(dst_pfn, 0);
            result = OPAL_ENOMEM;
            goto err;
        }
    }

    return OPAL_OK;

err:
    free_mapped_range(child->pagetable, entry.start, current_addr - entry.start);
    pagetable_unmap(child->pagetable, entry.start, current_addr - entry.start, false);
    vmtree_remove(&child->vmtree, entry.start, entry.end);
    kfree(dst_area, sizeof(*dst_area));
    return result;
}

static kerrno_t process_clone_vmem(struct process *child, struct process *parent) {
    struct vmtree_iter iter = vmtree_before_begin(&parent->vmtree);
    struct vmtree_entry entry;
    while (vmtree_iter_next(&iter, &entry)) {
        kerrno_t result = clone_vmem_range(child, parent, entry);
        if (!kerrno_ok(result)) {
            return result;
        }
    }

    return OPAL_OK;
}

static void fork_entry(void) {
    struct isr_stackframe *kstack = task_current()->kstack;
    stackframe_set_return_value(kstack, 0);
    return_to_userland(kstack);
}

kerrno_t process_fork(
    const struct isr_stackframe *frame, procptr_t *proc_out, taskptr_t *task_out) {
    struct process *parent = process_current();
    procptr_t child = process_create();
    if (!child.ptr) {
        return OPAL_ENOMEM;
    }

    kerrno_t result = OPAL_OK;

    irqlock_t irqlock = irqlock_acquire();
    result = filetable_clone(&child.ptr->open_files, &parent->open_files);
    irqlock_release(&irqlock);
    if (!kerrno_ok(result)) {
        goto err_child;
    }

    result = process_clone_vmem(child.ptr, parent);
    if (!kerrno_ok(result)) {
        goto err_child;
    }

    taskptr_t task = task_create(child.ptr, fork_entry, TASK_PRIORITY_NORMAL);
    if (!task.ptr) {
        result = OPAL_ENOMEM;
        goto err_child;
    }

    struct isr_stackframe *kstack = task.ptr->kstack;
    *kstack = *frame;

    *proc_out = child;
    *task_out = task;
    return OPAL_OK;

err_child:
    process_release(child);
    return result;
}

static page_entry_t elf_flags_to_ptbl_flags(uint32_t flags) {
    page_entry_t ptbl_flags = PTBL_USER;
    if (flags & ELF_PF_W) {
        ptbl_flags |= PTBL_WRITABLE;
    }
    return ptbl_flags;
}

static void free_mapped_range(struct pagetable *ptbl, virt_addr_t addr, virt_size_t mapped_len) {
    for (virt_size_t off = 0; off < mapped_len; off += PAGE_SIZE) {
        phys_addr_t pa;
        if (!pagetable_lookup(ptbl, addr + off, &pa)) {
            continue;
        }
        mm_free_page(phys_to_pfn(pa & PTE_MASK_ADDR), 0);
    }
}

static kerrno_t map_section(struct pagetable *ptbl, struct vmtree *vmtree, void *data,
    size_t filesz, virt_addr_t addr, virt_size_t memsz, uint32_t flags) {
    if (memsz == 0) {
        return OPAL_OK;
    }

    struct vm_area *area = kzalloc(sizeof(*area));
    if (!area) {
        return OPAL_ENOMEM;
    }
    area->ptbl_flags = elf_flags_to_ptbl_flags(flags);

    irqlock_t irqlock = irqlock_acquire();
    kerrno_t result = OPAL_ENOMEM;

    vmtree_status_t insertion = vmtree_insert(vmtree, addr, addr + memsz, area);
    if (insertion != VMTREE_OK) {
        if (insertion == VMTREE_ERR_EXISTS) {
            result = OPAL_EBADIMAGE;
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

        virt_addr_t map_rs =
            pagetable_map(ptbl, current_addr, pfn_to_phys(pfn), size, area->ptbl_flags);
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
    return OPAL_OK;

err_alloc:
    free_mapped_range(ptbl, addr, current_addr - addr);
    pagetable_unmap(ptbl, addr, current_addr - addr, false);
    vmtree_remove(vmtree, addr, addr + memsz);
err:
    kfree(area, sizeof(*area));
    irqlock_release(&irqlock);
    return result;
}

static void process_entry(void) {
    virt_addr_t *kstack = task_current()->kstack;
    enter_userland(kstack[0], kstack[1]);
}

static taskptr_t process_create_usertask_suspended(struct process *proc, virt_addr_t entry,
    virt_addr_t stack, virt_size_t stack_size, enum task_priority priority) {
    taskptr_t task = task_create(proc, process_entry, priority);
    if (!task.ptr) {
        return task;
    }

    virt_addr_t *kstack = task.ptr->kstack;
    kstack[0] = entry;
    kstack[1] = stack + stack_size;

    return task;
}

taskptr_t process_create_usertask(struct process *proc, virt_addr_t entry, virt_addr_t stack,
    virt_size_t stack_size, enum task_priority priority) {
    taskptr_t task = process_create_usertask_suspended(proc, entry, stack, stack_size, priority);
    if (task.ptr) {
        task_resume(task.ptr);
    }
    return task;
}

static kerrno_t load_elf_image(struct pagetable *ptbl, struct vmtree *vmtree, void *elf,
    size_t size, virt_addr_t *entry_out, virt_addr_t *stack_out, virt_size_t *stack_size_out) {
    if (size < sizeof(struct elf64_header)) {
        return OPAL_EBADIMAGE;
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
        return OPAL_EBADIMAGE;
    }

    if (hdr->version != ELF_VERSION) {
        return OPAL_EBADIMAGE;
    }
    if (hdr->eh_size < sizeof(*hdr)) {
        return OPAL_EBADIMAGE;
    }

    if (hdr->type != ELF_ET_EXEC) {
        return OPAL_ENOEXEC;
    }
    if (hdr->machine != ELF_X86_64) {
        return OPAL_ENOEXEC;
    }

    if (hdr->ph_ent_size < sizeof(struct elf64_phdr)) {
        return OPAL_EBADIMAGE;
    }
    if (hdr->ph_off >= size) {
        return OPAL_EBADIMAGE;
    }
    if (hdr->ph_num > (size - hdr->ph_off) / hdr->ph_ent_size) {
        return OPAL_EBADIMAGE;
    }
    if (hdr->ph_ent_size % alignof(struct elf64_phdr) != 0) {
        return OPAL_EBADIMAGE;
    }

    unsigned char *bytes = elf;
    virt_addr_t ustack_bottom = 0x0000400000000000;

    if (hdr->entry >= ustack_bottom) {
        return OPAL_ENOEXEC;
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
                return OPAL_ENOEXEC;
        }

        if (phdr->filesz > phdr->memsz) {
            return OPAL_EBADIMAGE;
        }
        if (phdr->offset > size) {
            return OPAL_EBADIMAGE;
        }
        if (phdr->filesz > size - phdr->offset) {
            return OPAL_EBADIMAGE;
        }
        if (phdr->vaddr + phdr->memsz < phdr->vaddr) {
            return OPAL_EBADIMAGE;
        }
        if (phdr->align != 0 && !ispower2(phdr->align)) {
            return OPAL_EBADIMAGE;
        }
        if (phdr->align > 1 && (phdr->vaddr % phdr->align != phdr->offset % phdr->align)) {
            return OPAL_EBADIMAGE;
        }
        if (phdr->vaddr % PAGE_SIZE != 0 || phdr->offset % PAGE_SIZE != 0) {
            return OPAL_ENOEXEC;
        }
        if (phdr->vaddr + phdr->memsz >= ustack_bottom) {
            return OPAL_ENOEXEC;
        }

        kerrno_t result = map_section(ptbl, vmtree, bytes + phdr->offset, phdr->filesz, phdr->vaddr,
            phdr->memsz, phdr->flags);
        if (!kerrno_ok(result)) {
            return result;
        }
    }

    kerrno_t result =
        map_section(ptbl, vmtree, NULL, 0, ustack_bottom, PAGE_SIZE, ELF_PF_R | ELF_PF_W);
    if (!kerrno_ok(result)) {
        return result;
    }

    *entry_out = hdr->entry;
    *stack_out = ustack_bottom;
    *stack_size_out = PAGE_SIZE;
    return OPAL_OK;
}

kerrno_t process_load_elf(struct process *proc, void *elf, size_t size, taskptr_t *out) {
    virt_addr_t entry;
    virt_addr_t stack;
    virt_size_t stack_size;
    kerrno_t result =
        load_elf_image(proc->pagetable, &proc->vmtree, elf, size, &entry, &stack, &stack_size);
    if (!kerrno_ok(result)) {
        return result;
    }

    *out = process_create_usertask(proc, entry, stack, stack_size, TASK_PRIORITY_NORMAL);
    if (!out->ptr) {
        return OPAL_ENOMEM;
    }

    return OPAL_OK;
}

static kerrno_t read_file_all(struct file *file, void **buf_out, size_t *len_out) {
    fs_ssize_t len = file->ops->seek(file, 0, FS_SEEK_END);
    if (!kerrno_ok(len)) {
        return fs_ssize_errno(len);
    }
    if (len == 0) {
        return OPAL_EBADIMAGE;
    }

    void *buf = kzalloc(len);
    if (!buf) {
        return OPAL_ENOMEM;
    }

    kerrno_t result = OPAL_OK;
    fs_ssize_t n = file->ops->read(file, &(fs_size_t){ 0 }, buf, len);
    if (n < 0) {
        result = fs_ssize_errno(n);
        goto err_buf;
    }
    if (n != len) {
        result = OPAL_EIO;
        goto err_buf;
    }

    *buf_out = buf;
    *len_out = len;
    return OPAL_OK;

err_buf:
    kfree(buf, len);
    return result;
}

kerrno_t process_exec_elf_file(struct process *proc, struct file *file, taskptr_t *task_out) {
    if (!process_has_current_task_only(proc)) {
        return OPAL_EBUSY;
    }

    void *buf;
    size_t len;
    kerrno_t result = read_file_all(file, &buf, &len);
    if (!kerrno_ok(result)) {
        return result;
    }

    struct process_image image;
    result = process_image_create(&image);
    if (!kerrno_ok(result)) {
        goto err_buf;
    }

    virt_addr_t entry;
    virt_addr_t stack;
    virt_size_t stack_size;
    result = load_elf_image(image.pagetable, &image.vmtree, buf, len, &entry, &stack, &stack_size);
    if (!kerrno_ok(result)) {
        goto err_image;
    }

    taskptr_t task =
        process_create_usertask_suspended(proc, entry, stack, stack_size, TASK_PRIORITY_NORMAL);
    if (!task.ptr) {
        result = OPAL_ENOMEM;
        goto err_image;
    }

    struct pagetable *old_ptbl = proc->pagetable;
    struct vmtree old_vmtree;
    vmtree_move(&old_vmtree, &proc->vmtree);

    proc->pagetable = image.pagetable;
    vmtree_move(&proc->vmtree, &image.vmtree);
    if (proc == process_current()) {
        pagetable_apply(proc->pagetable);
    }

    process_image_destroy_parts(old_ptbl, &old_vmtree);
    kfree(buf, len);

    *task_out = task;
    return OPAL_OK;

err_image:
    process_image_destroy(&image);
err_buf:
    kfree(buf, len);
    return result;
}

kerrno_t process_load_elf_file(struct file *file, procptr_t *proc_out, taskptr_t *task_out) {
    void *buf;
    size_t len;
    kerrno_t result = read_file_all(file, &buf, &len);
    if (!kerrno_ok(result)) {
        return result;
    }

    procptr_t proc = process_create();
    if (!proc.ptr) {
        result = OPAL_ENOMEM;
        goto err_buf;
    }

    taskptr_t task;
    result = process_load_elf(proc.ptr, buf, len, &task);
    if (!kerrno_ok(result)) {
        process_release(proc);
        goto err_buf;
    }

    kfree(buf, len);
    *proc_out = proc;
    *task_out = task;
    return OPAL_OK;

err_buf:
    kfree(buf, len);
    return result;
}
