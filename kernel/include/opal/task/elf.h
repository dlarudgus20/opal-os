#ifndef OPAL_TASK_ELF_H
#define OPAL_TASK_ELF_H

#include <stdint.h>

#define ELF_CLASSNONE   0
#define ELF_CLASS32     1
#define ELF_CLASS64     2
#define ELF_X86_64      62
#define ELF_DATANONE    0
#define ELF_DATA2LSB    1
#define ELF_DATA2MSB    2
#define ELF_VERSION     1
#define ELF_OSABI_NONE  0

#define ELF_ET_NONE 0
#define ELF_ET_REL  1
#define ELF_ET_EXEC 2
#define ELF_ET_DYN  3
#define ELF_ET_CORE 4

#define ELF_SHN_UNDEF       0
#define ELF_SHN_LORESERVE   0xff00
#define ELF_SHN_ABS         0xfff1
#define ELF_SHN_COMMON      0xfff2
#define ELF_SHN_XINDEX      0xffff

#define ELF_SHT_NULL        0
#define ELF_SHT_PROGBITS    1
#define ELF_SHT_SYMTAB      2
#define ELF_SHT_STRTAB      3
#define ELF_SHT_RELA        4
#define ELF_SHT_HASH        5
#define ELF_SHT_DYNAMIC     6
#define ELF_SHT_NOTE        7
#define ELF_SHT_NOBITS      8
#define ELF_SHT_REL         9
#define ELF_SHT_SHLIB       10
#define ELF_SHT_DYNSYM      11
#define ELF_SHT_INIT        14
#define ELF_SHT_FINI        15
#define ELF_SHT_PREINIT     16
#define ELF_SHT_GROUP       17
#define ELF_SHT_SYMTAB_SHNDX 18

#define ELF_SHF_WRITE       0x001
#define ELF_SHF_ALLOC       0x002
#define ELF_SHF_EXEC        0x004
#define ELF_SHF_MERGE       0x010
#define ELF_SHF_STRINGS     0x020
#define ELF_SHF_INFO_LINK   0x040
#define ELF_SHT_LINK_ORDER  0x080
#define ELF_SHF_OS_NONCONFORMING 0x100
#define ELF_SHF_GROUP       0x200
#define ELF_SHF_TLS         0x400

#define ELF_STN_UNDEF 0

#define ELF_PT_NULL     0
#define ELF_PT_LOAD     1
#define ELF_PT_DYNAMIC  2
#define ELF_PT_INTERP   3
#define ELF_PT_NOTE     4
#define ELF_PT_SHLIB    5
#define ELF_PT_PHDR     6
#define ELF_PT_TLS      7

#define ELF_PF_X    1
#define ELF_PF_W    2
#define ELF_PF_R    4

struct elf_ident {
    unsigned char magic[4];
    unsigned char elf_class;
    unsigned char encoding;
    unsigned char elf_version;
    unsigned char os_abi;
    unsigned char abi_version;
    unsigned char reserved[7];
};

struct elf64_header {
    struct elf_ident ident;
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t ph_off;
    uint64_t sh_off;
    uint32_t flags;
    uint16_t eh_size;
    uint16_t ph_ent_size;
    uint16_t ph_num;
    uint16_t sh_ent_size;
    uint16_t sh_num;
    uint16_t sh_strndx;
};

struct elf64_shdr {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t addr_align;
    uint64_t ent_size;
};

struct elf64_sym {
    uint32_t name;
    unsigned char info;
    unsigned char other;
    uint16_t shndx;
    uint64_t value;
    uint64_t size;
};

struct elf64_rel {
    uint64_t offest;
    uint64_t info;
};

struct elf64_rela {
    uint64_t offset;
    uint64_t info;
    int64_t addend;
};

struct elf64_phdr {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};

#endif
