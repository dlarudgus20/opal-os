#ifndef OPAL_KARGS_H
#define OPAL_KARGS_H

struct bootinfo_module;

struct kargs {
    const struct bootinfo_module *initramfs;
};

void kargs_init(void);
const struct kargs *kargs_get(void);
void kargs_postboot(void);
void kargs_print_log(void);

#endif
