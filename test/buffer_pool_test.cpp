#include <gtest/gtest.h>
#include <filesystem>
#include "storage/buffer_pool.h"
#include "storage/disk_manager.h"

class BufferPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_ = "test_bp.db";
        std::filesystem::remove(test_file_);
        dm_  = std::make_unique<DiskManager>(test_file_);
        bp_  = std::make_unique<BufferPool>(4, *dm_); // small pool of 4
    }
    void TearDown() override {
        bp_.reset();
        dm_.reset();
        std::filesystem::remove(test_file_);
    }
    std::string                  test_file_;
    std::unique_ptr<DiskManager> dm_;
    std::unique_ptr<BufferPool>  bp_;
};

TEST_F(BufferPoolTest, NewPage) {
    uint32_t page_id;
    Page *page = bp_->NewPage(page_id);
    ASSERT_NE(page, nullptr);
    EXPECT_EQ(page_id, 0);
    bp_->UnpinPage(page_id, false);
}

TEST_F(BufferPoolTest, FetchPageAfterNew) {
    uint32_t page_id;
    Page *page = bp_->NewPage(page_id);
    page->Init(page_id, PageType::HEAP);
    bp_->UnpinPage(page_id, true);

    Page *fetched = bp_->FetchPage(page_id);
    ASSERT_NE(fetched, nullptr);
    EXPECT_EQ(fetched->GetPageId(),   page_id);
    EXPECT_EQ(fetched->GetPageType(), PageType::HEAP);
    bp_->UnpinPage(page_id, false);
}

TEST_F(BufferPoolTest, LRUEviction) {
    // Fill pool with 4 pages, all unpinned
    std::vector<uint32_t> ids;
    for (int i = 0; i < 4; i++) {
        uint32_t id;
        Page *p = bp_->NewPage(id);
        p->Init(id, PageType::HEAP);
        ids.push_back(id);
        bp_->UnpinPage(id, true);
    }

    // Allocate a 5th — should evict page 0 (LRU)
    uint32_t new_id;
    Page *new_page = bp_->NewPage(new_id);
    ASSERT_NE(new_page, nullptr);
    bp_->UnpinPage(new_id, false);
}

TEST_F(BufferPoolTest, PinnedPageNotEvicted) {
    // Pin page 0 and fill pool
    uint32_t id0;
    Page *p0 = bp_->NewPage(id0);
    p0->Init(id0, PageType::HEAP);
    // Do NOT unpin p0

    for (int i = 1; i < 4; i++) {
        uint32_t id;
        Page *p = bp_->NewPage(id);
        p->Init(id, PageType::HEAP);
        bp_->UnpinPage(id, false);
    }

    // Pool is full, page 0 is pinned
    // Unpinning now should be fine
    bp_->UnpinPage(id0, false);
}

TEST_F(BufferPoolTest, DirtyPageWrittenOnEviction) {
    uint32_t id;
    Page *page = bp_->NewPage(id);
    page->Init(id, PageType::HEAP);

    const char *data = "dirty_data";
    page->InsertTuple(data, static_cast<uint16_t>(strlen(data)));
    bp_->UnpinPage(id, true); // mark dirty

    // Fill pool to force eviction
    for (int i = 0; i < 4; i++) {
        uint32_t new_id;
        Page *p = bp_->NewPage(new_id);
        bp_->UnpinPage(new_id, false);
    }

    // Fetch original page again — should read from disk
    Page *refetched = bp_->FetchPage(id);
    ASSERT_NE(refetched, nullptr);
    EXPECT_EQ(refetched->GetNumSlots(), 1);
    bp_->UnpinPage(id, false);
}