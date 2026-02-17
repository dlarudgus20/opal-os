#include <gtest/gtest.h>

#include <array>

extern "C" {
#define restrict
#include <opal/mm/map.h>
}

static struct mmap sanitize_with_capacity(
    const struct mmap_entry *in_entries,
    uint32_t in_len,
    struct mmap_entry *out_entries,
    uint32_t out_cap
) {
    struct mmap in_map = {
        .entries = const_cast<struct mmap_entry *>(in_entries),
        .length = in_len,
    };
    struct mmap out_map = {
        .entries = out_entries,
        .length = 0,
    };

    construct_usable_map(&out_map, out_cap, &in_map);
    return out_map;
}

TEST(BootMapSanitizeTest, FiltersAlignsSortsAndMergesUsableOnly) {
    std::array<struct mmap_entry, 8> in = {{
        {.addr = 0x3000, .len = 0x1000, .type = MMAP_ENTRY_USABLE},
        {.addr = 0x1001, .len = 0x3000, .type = MMAP_ENTRY_USABLE},
        {.addr = 0x7000, .len = 0x1000, .type = MMAP_ENTRY_RESERVED},
        {.addr = 0x5000, .len = 0x1000, .type = MMAP_ENTRY_USABLE},
        {.addr = 0x6000, .len = 0x0000, .type = MMAP_ENTRY_USABLE},
        {.addr = 0x4fff, .len = 0x2002, .type = MMAP_ENTRY_USABLE},
        {.addr = 0x9000, .len = 0x1fff, .type = MMAP_ENTRY_USABLE},
        {.addr = 0x8000, .len = 0x1000, .type = MMAP_ENTRY_USABLE},
    }};
    std::array<struct mmap_entry, 16> out_storage = {};

    const struct mmap out = sanitize_with_capacity(in.data(), in.size(), out_storage.data(), out_storage.size());

    ASSERT_EQ(out.length, 4u);

    EXPECT_EQ(out.entries[0].addr, 0x2000u);
    EXPECT_EQ(out.entries[0].len, 0);
    EXPECT_EQ(out.entries[0].type, USABLE_ENTRY_METADATA);

    EXPECT_EQ(out.entries[1].addr, 0x2000u);
    EXPECT_EQ(out.entries[1].len, 0x2000u);
    EXPECT_EQ(out.entries[1].type, MMAP_ENTRY_USABLE);

    EXPECT_EQ(out.entries[2].addr, 0x5000u);
    EXPECT_EQ(out.entries[2].len, 0x2000u);
    EXPECT_EQ(out.entries[2].type, MMAP_ENTRY_USABLE);

    EXPECT_EQ(out.entries[3].addr, 0x8000u);
    EXPECT_EQ(out.entries[3].len, 0x2000u);
    EXPECT_EQ(out.entries[3].type, MMAP_ENTRY_USABLE);
}

TEST(BootMapSanitizeTest, KeepsTopRangeWhenInputEndOverflows) {
    std::array<struct mmap_entry, 1> in = {{
        {.addr = PHYS_ADDR_MAX - 0x1000 + 1, .len = 0x3000, .type = MMAP_ENTRY_USABLE},
    }};
    std::array<struct mmap_entry, 4> out_storage = {};

    const struct mmap out = sanitize_with_capacity(in.data(), in.size(), out_storage.data(), out_storage.size());

    ASSERT_EQ(out.length, 2u);
    EXPECT_EQ(out.entries[0].addr, PHYS_ADDR_MAX - 0x1000 + 1);
    EXPECT_EQ(out.entries[0].len, 0);
    EXPECT_EQ(out.entries[0].type, USABLE_ENTRY_METADATA);
    EXPECT_EQ(out.entries[1].addr, PHYS_ADDR_MAX - 0x1000 + 1);
    EXPECT_EQ(out.entries[1].len, 0x1000u);
    EXPECT_EQ(out.entries[1].type, MMAP_ENTRY_USABLE);
}

TEST(BootMapSanitizeDeathTest, RespectsOutputCapacity) {
    std::array<struct mmap_entry, 3> in = {{
        {.addr = 0x1000, .len = 0x1000, .type = MMAP_ENTRY_USABLE},
        {.addr = 0x4000, .len = 0x1000, .type = MMAP_ENTRY_USABLE},
        {.addr = 0x7000, .len = 0x1000, .type = MMAP_ENTRY_USABLE},
    }};
    std::array<struct mmap_entry, 3> out_storage = {};

    EXPECT_DEATH({
        sanitize_with_capacity(in.data(), in.size(), out_storage.data(), out_storage.size());
    }, "usable_map entry buffer overflow");
}
