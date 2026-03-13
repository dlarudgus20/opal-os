#include "test-pch.h"

extern "C" {
#define restrict
#include <opal/mm/tmpalloc.h>
#include <opal/platform/mm/defines.h>
}

TEST(TmpallocTest, InsertsMetadataBeforeUsable) {
    std::array<struct mmap_entry, 8> entries = {{
        {.addr = 0x1000, .len = 0x1000, .type = MM_SEC_ENTRY_RESERVED},
        {.addr = 0x2000, .len = 0x4000, .type = MM_SEC_ENTRY_USABLE},
    }};

    struct mmap src = {
        .entries = entries.data(),
        .length = 2,
    };

    struct tmpalloc ta;
    tmpalloc_create(&ta, entries.data(), entries.size(), &src);

    size_t allocated_pages = 0;
    const phys_addr_t addr = tmpalloc_alloc_pages(&ta, 1, &allocated_pages);

    ASSERT_EQ(addr, 0x2000u);
    ASSERT_EQ(allocated_pages, 1u);
    ASSERT_EQ(ta.mm.length, 3u);

    EXPECT_EQ(ta.mm.entries[0].type, MM_SEC_ENTRY_RESERVED);
    EXPECT_EQ(ta.mm.entries[0].addr, 0x1000u);
    EXPECT_EQ(ta.mm.entries[0].len, 0x1000u);

    EXPECT_EQ(ta.mm.entries[1].type, MM_SEC_ENTRY_METADATA);
    EXPECT_EQ(ta.mm.entries[1].addr, 0x2000u);
    EXPECT_EQ(ta.mm.entries[1].len, PAGE_SIZE);

    EXPECT_EQ(ta.mm.entries[2].type, MM_SEC_ENTRY_USABLE);
    EXPECT_EQ(ta.mm.entries[2].addr, 0x3000u);
    EXPECT_EQ(ta.mm.entries[2].len, 0x3000u);
}

TEST(TmpallocTest, MergesIntoPreviousMetadata) {
    std::array<struct mmap_entry, 8> entries = {{
        {.addr = 0x2000, .len = 0x1000, .type = MM_SEC_ENTRY_METADATA},
        {.addr = 0x3000, .len = 0x2000, .type = MM_SEC_ENTRY_USABLE},
    }};

    struct mmap src = {
        .entries = entries.data(),
        .length = 2,
    };

    struct tmpalloc ta;
    tmpalloc_create(&ta, entries.data(), entries.size(), &src);

    size_t allocated_pages = 0;
    const phys_addr_t addr = tmpalloc_alloc_pages(&ta, 1, &allocated_pages);

    ASSERT_EQ(addr, 0x3000u);
    ASSERT_EQ(allocated_pages, 1u);
    ASSERT_EQ(ta.mm.length, 2u);

    EXPECT_EQ(ta.mm.entries[0].type, MM_SEC_ENTRY_METADATA);
    EXPECT_EQ(ta.mm.entries[0].addr, 0x2000u);
    EXPECT_EQ(ta.mm.entries[0].len, 0x2000u);

    EXPECT_EQ(ta.mm.entries[1].type, MM_SEC_ENTRY_USABLE);
    EXPECT_EQ(ta.mm.entries[1].addr, 0x4000u);
    EXPECT_EQ(ta.mm.entries[1].len, 0x1000u);
}

TEST(TmpallocDeathTest, PanicsWhenEntryInsertionExceedsCapacity) {
    std::array<struct mmap_entry, 2> entries = {{
        {.addr = 0x1000, .len = 0x1000, .type = MM_SEC_ENTRY_RESERVED},
        {.addr = 0x2000, .len = 0x2000, .type = MM_SEC_ENTRY_USABLE},
    }};

    struct mmap src = {
        .entries = entries.data(),
        .length = 2,
    };

    struct tmpalloc ta;
    tmpalloc_create(&ta, entries.data(), 2, &src);

    EXPECT_DEATH({
        size_t allocated_pages = 0;
        (void)tmpalloc_alloc_pages(&ta, 1, &allocated_pages);
    }, "too many mmap entries");
}

TEST(TmpallocTest, AllocatesMultiplePagesWhenMaxPagesIsLargerThanOne) {
    std::array<struct mmap_entry, 4> entries = {{
        {.addr = 0x2000, .len = 0x5000, .type = MM_SEC_ENTRY_USABLE},
    }};

    struct mmap src = {
        .entries = entries.data(),
        .length = 1,
    };

    struct tmpalloc ta;
    tmpalloc_create(&ta, entries.data(), entries.size(), &src);

    size_t allocated_pages = 0;
    const phys_addr_t addr = tmpalloc_alloc_pages(&ta, 3, &allocated_pages);

    ASSERT_EQ(addr, 0x2000u);
    ASSERT_EQ(allocated_pages, 3u);
    ASSERT_EQ(ta.mm.length, 2u);
    EXPECT_EQ(ta.mm.entries[0].type, MM_SEC_ENTRY_METADATA);
    EXPECT_EQ(ta.mm.entries[0].len, 3u * PAGE_SIZE);
    EXPECT_EQ(ta.mm.entries[1].type, MM_SEC_ENTRY_USABLE);
    EXPECT_EQ(ta.mm.entries[1].addr, 0x5000u);
    EXPECT_EQ(ta.mm.entries[1].len, 0x2000u);
}

TEST(TmpallocTest, RemovesUsableEntryWhenFullyConsumed) {
    std::array<struct mmap_entry, 4> entries = {{
        {.addr = 0x1000, .len = 0x2000, .type = MM_SEC_ENTRY_RESERVED},
        {.addr = 0x3000, .len = 0x1000, .type = MM_SEC_ENTRY_USABLE},
    }};

    struct mmap src = {
        .entries = entries.data(),
        .length = 2,
    };

    struct tmpalloc ta;
    tmpalloc_create(&ta, entries.data(), entries.size(), &src);

    size_t allocated_pages = 0;
    const phys_addr_t addr = tmpalloc_alloc_pages(&ta, 1, &allocated_pages);

    ASSERT_EQ(addr, 0x3000u);
    ASSERT_EQ(allocated_pages, 1u);
    ASSERT_EQ(ta.mm.length, 2u);
    EXPECT_EQ(ta.mm.entries[1].type, MM_SEC_ENTRY_METADATA);
    EXPECT_EQ(ta.mm.entries[1].addr, 0x3000u);
    EXPECT_EQ(ta.mm.entries[1].len, PAGE_SIZE);
}

TEST(TmpallocTest, SkipsSubPageUsableAndAllocatesFromLaterEntry) {
    std::array<struct mmap_entry, 6> entries = {{
        {.addr = 0x1000, .len = 0x1000, .type = MM_SEC_ENTRY_RESERVED},
        {.addr = 0x2000, .len = 0x0800, .type = MM_SEC_ENTRY_USABLE},
        {.addr = 0x3000, .len = 0x1000, .type = MM_SEC_ENTRY_RESERVED},
        {.addr = 0x4000, .len = 0x2000, .type = MM_SEC_ENTRY_USABLE},
    }};

    struct mmap src = {
        .entries = entries.data(),
        .length = 4,
    };

    struct tmpalloc ta;
    tmpalloc_create(&ta, entries.data(), entries.size(), &src);

    size_t allocated_pages = 0;
    const phys_addr_t addr = tmpalloc_alloc_pages(&ta, 1, &allocated_pages);

    ASSERT_EQ(addr, 0x4000u);
    ASSERT_EQ(allocated_pages, 1u);
    ASSERT_EQ(ta.mm.length, 5u);
    EXPECT_EQ(ta.mm.entries[1].type, MM_SEC_ENTRY_USABLE);
    EXPECT_EQ(ta.mm.entries[1].len, 0x0800u);
    EXPECT_EQ(ta.mm.entries[3].type, MM_SEC_ENTRY_METADATA);
    EXPECT_EQ(ta.mm.entries[3].addr, 0x4000u);
    EXPECT_EQ(ta.mm.entries[4].type, MM_SEC_ENTRY_USABLE);
    EXPECT_EQ(ta.mm.entries[4].addr, 0x5000u);
}

TEST(TmpallocTest, RepeatedAllocationsMergeMetadataRun) {
    std::array<struct mmap_entry, 8> entries = {{
        {.addr = 0x1000, .len = 0x4000, .type = MM_SEC_ENTRY_USABLE},
    }};

    struct mmap src = {
        .entries = entries.data(),
        .length = 1,
    };

    struct tmpalloc ta;
    tmpalloc_create(&ta, entries.data(), entries.size(), &src);

    size_t allocated_pages = 0;
    EXPECT_EQ(tmpalloc_alloc_pages(&ta, 1, &allocated_pages), 0x1000u);
    EXPECT_EQ(allocated_pages, 1u);
    EXPECT_EQ(tmpalloc_alloc_pages(&ta, 1, &allocated_pages), 0x2000u);
    EXPECT_EQ(allocated_pages, 1u);

    ASSERT_EQ(ta.mm.length, 2u);
    EXPECT_EQ(ta.mm.entries[0].type, MM_SEC_ENTRY_METADATA);
    EXPECT_EQ(ta.mm.entries[0].addr, 0x1000u);
    EXPECT_EQ(ta.mm.entries[0].len, 0x2000u);
    EXPECT_EQ(ta.mm.entries[1].type, MM_SEC_ENTRY_USABLE);
    EXPECT_EQ(ta.mm.entries[1].addr, 0x3000u);
    EXPECT_EQ(ta.mm.entries[1].len, 0x2000u);
}

TEST(TmpallocDeathTest, PanicsWhenNoUsablePageExists) {
    std::array<struct mmap_entry, 3> entries = {{
        {.addr = 0x1000, .len = 0x1000, .type = MM_SEC_ENTRY_RESERVED},
        {.addr = 0x2000, .len = 0x0800, .type = MM_SEC_ENTRY_USABLE},
    }};

    struct mmap src = {
        .entries = entries.data(),
        .length = 2,
    };

    struct tmpalloc ta;
    tmpalloc_create(&ta, entries.data(), entries.size(), &src);

    EXPECT_DEATH({
        size_t allocated_pages = 0;
        (void)tmpalloc_alloc_pages(&ta, 1, &allocated_pages);
    }, "mm_section has no usable page for tmpalloc");
}
