#include <gtest/gtest.h>
#include <cstring>
#include <filesystem>
#include "storage/page.h"
#include "storage/disk_manager.h"

// ---- Page tests ----

TEST(PageTest, InitPage) {
    Page page;
    page.Init(42, PageType::HEAP);
    EXPECT_EQ(page.GetPageId(),   42);
    EXPECT_EQ(page.GetPageType(), PageType::HEAP);
    EXPECT_EQ(page.GetNumSlots(), 0);
    EXPECT_EQ(page.GetFreeSpace(), PAGE_SIZE - PAGE_HEADER_SIZE);
}

TEST(PageTest, InsertAndGetTuple) {
    Page page;
    page.Init(1, PageType::HEAP);

    const char *data = "hello_world";
    uint16_t    len  = static_cast<uint16_t>(strlen(data));

    uint16_t slot_id = page.InsertTuple(data, len);
    EXPECT_EQ(slot_id, 0);
    EXPECT_EQ(page.GetNumSlots(), 1);

    uint16_t    out_size = 0;
    const char *retrieved = page.GetTuple(0, out_size);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(out_size, len);
    EXPECT_EQ(memcmp(retrieved, data, len), 0);
}

TEST(PageTest, InsertMultipleTuples) {
    Page page;
    page.Init(1, PageType::HEAP);

    for (int i = 0; i < 10; i++) {
        std::string s = "tuple_" + std::to_string(i);
        uint16_t slot = page.InsertTuple(s.c_str(),
                                         static_cast<uint16_t>(s.size()));
        EXPECT_EQ(slot, i);
    }
    EXPECT_EQ(page.GetNumSlots(), 10);
}

TEST(PageTest, DeleteTuple) {
    Page page;
    page.Init(1, PageType::HEAP);

    const char *data = "test_data";
    page.InsertTuple(data, static_cast<uint16_t>(strlen(data)));
    page.DeleteTuple(0);

    uint16_t    size = 0;
    const char *retrieved = page.GetTuple(0, size);
    EXPECT_EQ(retrieved, nullptr);
}

TEST(PageTest, FreeSpaceDecreases) {
    Page page;
    page.Init(1, PageType::HEAP);
    uint16_t initial = page.GetFreeSpace();

    const char *data = "some_data";
    page.InsertTuple(data, static_cast<uint16_t>(strlen(data)));

    EXPECT_LT(page.GetFreeSpace(), initial);
}

// ---- DiskManager tests ----

class DiskManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_ = "test_disk.db";
        std::filesystem::remove(test_file_);
    }
    void TearDown() override {
        std::filesystem::remove(test_file_);
    }
    std::string test_file_;
};

TEST_F(DiskManagerTest, AllocateAndReadWrite) {
    DiskManager dm(test_file_);
    EXPECT_EQ(dm.GetNumPages(), 0);

    uint32_t page_id = dm.AllocatePage();
    EXPECT_EQ(page_id, 0);
    EXPECT_EQ(dm.GetNumPages(), 1);

    char write_buf[PAGE_SIZE];
    memset(write_buf, 0xAB, PAGE_SIZE);
    dm.WritePage(0, write_buf);

    char read_buf[PAGE_SIZE];
    dm.ReadPage(0, read_buf);
    EXPECT_EQ(memcmp(write_buf, read_buf, PAGE_SIZE), 0);
}

TEST_F(DiskManagerTest, MultiplePages) {
    DiskManager dm(test_file_);
    for (int i = 0; i < 5; i++) {
        uint32_t id = dm.AllocatePage();
        EXPECT_EQ(id, static_cast<uint32_t>(i));
    }
    EXPECT_EQ(dm.GetNumPages(), 5);
}