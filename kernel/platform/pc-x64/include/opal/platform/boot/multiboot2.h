/*   multiboot2.h - Multiboot 2 header file. */
/*   Copyright (C) 1999,2003,2007,2008,2009,2010  Free Software Foundation, Inc.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to
 *  deal in the Software without restriction, including without limitation the
 *  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 *  sell copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL ANY
 *  DEVELOPER OR DISTRIBUTOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 *  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef OPAL_PLATFORM_BOOT_MULTIBOOT2_H
#define OPAL_PLATFORM_BOOT_MULTIBOOT2_H

#include <stdint.h>

#define MULTIBOOT_SEARCH                    32768
#define MULTIBOOT_HEADER_ALIGN              8
#define MULTIBOOT2_HEADER_MAGIC             0xe85250d6
#define MULTIBOOT2_BOOTLOADER_MAGIC         0x36d76289
#define MULTIBOOT_MOD_ALIGN                 0x00001000
#define MULTIBOOT_INFO_ALIGN                0x00000008

#define MULTIBOOT_TAG_ALIGN                 8
#define MULTIBOOT_TAG_TYPE_END              0
#define MULTIBOOT_TAG_TYPE_CMDLINE          1
#define MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME 2
#define MULTIBOOT_TAG_TYPE_MODULE           3
#define MULTIBOOT_TAG_TYPE_BASIC_MEMINFO    4
#define MULTIBOOT_TAG_TYPE_BOOTDEV          5
#define MULTIBOOT_TAG_TYPE_MMAP             6
#define MULTIBOOT_TAG_TYPE_VBE              7
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER      8
#define MULTIBOOT_TAG_TYPE_ELF_SECTIONS     9
#define MULTIBOOT_TAG_TYPE_APM              10
#define MULTIBOOT_TAG_TYPE_EFI32            11
#define MULTIBOOT_TAG_TYPE_EFI64            12
#define MULTIBOOT_TAG_TYPE_SMBIOS           13
#define MULTIBOOT_TAG_TYPE_ACPI_OLD         14
#define MULTIBOOT_TAG_TYPE_ACPI_NEW         15
#define MULTIBOOT_TAG_TYPE_NETWORK          16
#define MULTIBOOT_TAG_TYPE_EFI_MMAP         17
#define MULTIBOOT_TAG_TYPE_EFI_BS           18
#define MULTIBOOT_TAG_TYPE_EFI32_IH         19
#define MULTIBOOT_TAG_TYPE_EFI64_IH         20
#define MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR   21

#define MULTIBOOT_HEADER_TAG_END                    0
#define MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST    1
#define MULTIBOOT_HEADER_TAG_ADDRESS                2
#define MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS          3
#define MULTIBOOT_HEADER_TAG_CONSOLE_FLAGS          4
#define MULTIBOOT_HEADER_TAG_FRAMEBUFFER            5
#define MULTIBOOT_HEADER_TAG_MODULE_ALIGN           6
#define MULTIBOOT_HEADER_TAG_EFI_BS                 7
#define MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI32    8
#define MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI64    9
#define MULTIBOOT_HEADER_TAG_RELOCATABLE            10

#define MULTIBOOT_ARCHITECTURE_I386     0
#define MULTIBOOT_ARCHITECTURE_MIPS32   4
#define MULTIBOOT_HEADER_TAG_OPTIONAL   1

#define MULTIBOOT_LOAD_PREFERENCE_NONE  0
#define MULTIBOOT_LOAD_PREFERENCE_LOW   1
#define MULTIBOOT_LOAD_PREFERENCE_HIGH  2

#define MULTIBOOT_CONSOLE_FLAGS_CONSOLE_REQUIRED    1
#define MULTIBOOT_CONSOLE_FLAGS_EGA_TEXT_SUPPORTED  2

typedef uint8_t mb_uint8_t;
typedef uint16_t mb_uint16_t;
typedef uint32_t mb_uint32_t;
typedef uint64_t mb_uint64_t;

struct mb_header {
    mb_uint32_t magic;
    mb_uint32_t architecture;
    mb_uint32_t header_length;
    mb_uint32_t checksum;
};

struct mb_header_tag {
    mb_uint16_t type;
    mb_uint16_t flags;
    mb_uint32_t size;
};

struct mb_header_tag_information_request {
    struct mb_header_tag tag;
    mb_uint32_t requests[];
};

struct mb_header_tag_address {
    struct mb_header_tag tag;
    mb_uint32_t header_addr;
    mb_uint32_t load_addr;
    mb_uint32_t load_end_addr;
    mb_uint32_t bss_end_addr;
};

struct mb_header_tag_entry_address {
    struct mb_header_tag tag;
    mb_uint32_t entry_addr;
};

struct mb_header_tag_console_flags {
    struct mb_header_tag tag;
    mb_uint32_t console_flags;
};

struct mb_header_tag_framebuffer {
    struct mb_header_tag tag;
    mb_uint32_t width;
    mb_uint32_t height;
    mb_uint32_t depth;
};

struct mb_header_tag_module_align {
    struct mb_header_tag tag;
};

struct mb_header_tag_relocatable {
    struct mb_header_tag tag;
    mb_uint32_t min_addr;
    mb_uint32_t max_addr;
    mb_uint32_t align;
    mb_uint32_t preference;
};

struct mb_color {
    mb_uint8_t red;
    mb_uint8_t green;
    mb_uint8_t blue;
};

#define MULTIBOOT_MEMORY_AVAILABLE          1
#define MULTIBOOT_MEMORY_RESERVED           2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE   3
#define MULTIBOOT_MEMORY_NVS                4
#define MULTIBOOT_MEMORY_BADRAM             5

struct mb_mmap_entry {
    mb_uint64_t addr;
    mb_uint64_t len;
    mb_uint32_t type;
    mb_uint32_t zero;
};

struct mb_tag {
    mb_uint32_t type;
    mb_uint32_t size;
};

struct mb_tag_string {
    struct mb_tag tag;
    char string[];
};

struct mb_tag_module {
    struct mb_tag tag;
    mb_uint32_t mod_start;
    mb_uint32_t mod_end;
    char cmdline[];
};

struct mb_tag_basic_meminfo {
    struct mb_tag tag;
    mb_uint32_t mem_lower;
    mb_uint32_t mem_upper;
};

struct mb_tag_bootdev {
    struct mb_tag tag;
    mb_uint32_t biosdev;
    mb_uint32_t slice;
    mb_uint32_t part;
};

struct mb_tag_mmap {
    struct mb_tag tag;
    mb_uint32_t entry_size;
    mb_uint32_t entry_version;
};

struct mb_vbe_info_block {
    mb_uint8_t external_specification[512];
};

struct mb_vbe_mode_info_block {
    mb_uint8_t external_specification[256];
};

struct mb_tag_vbe {
    struct mb_tag tag;

    mb_uint16_t vbe_mode;
    mb_uint16_t vbe_interface_seg;
    mb_uint16_t vbe_interface_off;
    mb_uint16_t vbe_interface_len;

    struct mb_vbe_info_block vbe_control_info;
    struct mb_vbe_mode_info_block vbe_mode_info;
};

#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED  0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB      1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT 2

struct mb_tag_framebuffer {
    struct mb_tag tag;

    mb_uint64_t framebuffer_addr;
    mb_uint32_t framebuffer_pitch;
    mb_uint32_t framebuffer_width;
    mb_uint32_t framebuffer_height;
    mb_uint8_t framebuffer_bpp;
    mb_uint8_t framebuffer_type;
    mb_uint16_t reserved;
};

struct mb_tag_framebuffer_indexed {
    mb_uint16_t framebuffer_palette_num_colors;
    struct mb_color framebuffer_palette[];
};

struct mb_tag_framebuffer_direct {
    mb_uint8_t framebuffer_red_field_position;
    mb_uint8_t framebuffer_red_mask_size;
    mb_uint8_t framebuffer_green_field_position;
    mb_uint8_t framebuffer_green_mask_size;
    mb_uint8_t framebuffer_blue_field_position;
    mb_uint8_t framebuffer_blue_mask_size;
};

struct mb_tag_elf_sections {
    struct mb_tag tag;
    mb_uint32_t num;
    mb_uint32_t entsize;
    mb_uint32_t shndx;
    char sections[];
};

struct mb_tag_apm {
    struct mb_tag tag;
    mb_uint16_t version;
    mb_uint16_t cseg;
    mb_uint32_t offset;
    mb_uint16_t cseg_16;
    mb_uint16_t dseg;
    mb_uint16_t flags;
    mb_uint16_t cseg_len;
    mb_uint16_t cseg_16_len;
    mb_uint16_t dseg_len;
};

struct mb_tag_efi32 {
    struct mb_tag tag;
    mb_uint32_t pointer;
};

struct mb_tag_efi64 {
    struct mb_tag tag;
    mb_uint64_t pointer;
};

struct mb_tag_smbios {
    struct mb_tag tag;
    mb_uint8_t major;
    mb_uint8_t minor;
    mb_uint8_t reserved[6];
    mb_uint8_t tables[];
};

struct mb_tag_old_acpi {
    struct mb_tag tag;
    mb_uint8_t rsdp[];
};

struct mb_tag_new_acpi {
    struct mb_tag tag;
    mb_uint8_t rsdp[];
};

struct mb_tag_network {
    struct mb_tag tag;
    mb_uint8_t dhcpack[];
};

struct mb_tag_efi_mmap {
    struct mb_tag tag;
    mb_uint32_t descr_size;
    mb_uint32_t descr_vers;
    mb_uint8_t efi_mmap[];
};

struct mb_tag_efi32_ih {
    struct mb_tag tag;
    mb_uint32_t pointer;
};

struct mb_tag_efi64_ih {
    struct mb_tag tag;
    mb_uint64_t pointer;
};

struct mb_tag_load_base_addr {
    struct mb_tag tag;
    mb_uint32_t load_base_addr;
};

#endif
