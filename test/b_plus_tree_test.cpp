#include <gtest/gtest.h>
#include <filesystem>
#include <algorithm>
#include "index/b_plus_tree.h"
#include "storage/buffer_pool.h"
#include "storage/disk_manager.h"

class BPlusTreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_ = "test_btree.db";
        std::filesystem::remove(test_file_);
        dm_   = std::make_unique<DiskManager>(test_file_);
        bp_   = std::make_unique<BufferPool>(64, *dm_);
        tree_ = std::make_unique<BPlusTree>(*bp_);
    }
    void TearDown() override {
        tree_.reset();
        bp_.reset();
        dm_.reset();
        std::filesystem::remove(test_file_);
    }
    std::string                  test_file_;
    std::unique_ptr<DiskManager> dm_;
    std::unique_ptr<BufferPool>  bp_;
    std::unique_ptr<BPlusTree>   tree_;
};

TEST_F(BPlusTreeTest, InsertAndSearch) {
    RID rid(1, 0);
    EXPECT_TRUE(tree_->Insert(42, rid));

    auto result = tree_->Search(42);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->page_id, 1);
    EXPECT_EQ(result->slot_id, 0);
}

TEST_F(BPlusTreeTest, SearchNotFound) {
    auto result = tree_->Search(99);
    EXPECT_FALSE(result.has_value());
}

TEST_F(BPlusTreeTest, DuplicateKeyRejected) {
    RID rid(1, 0);
    EXPECT_TRUE(tree_->Insert(10, rid));
    EXPECT_FALSE(tree_->Insert(10, rid)); // duplicate
}

TEST_F(BPlusTreeTest, Insert1000Keys) {
    for (int i = 0; i < 1000; i++) {
        RID rid(i, 0);
        EXPECT_TRUE(tree_->Insert(i, rid));
    }
    for (int i = 0; i < 1000; i++) {
        auto result = tree_->Search(i);
        ASSERT_TRUE(result.has_value()) << "Key not found: " << i;
        EXPECT_EQ(result->page_id, static_cast<uint32_t>(i));
    }
}

TEST_F(BPlusTreeTest, InsertDescendingOrder) {
    for (int i = 999; i >= 0; i--) {
        RID rid(i, 0);
        EXPECT_TRUE(tree_->Insert(i, rid));
    }
    for (int i = 0; i < 1000; i++) {
        auto result = tree_->Search(i);
        ASSERT_TRUE(result.has_value()) << "Key not found: " << i;
    }
}

TEST_F(BPlusTreeTest, RangeScan) {
    for (int i = 0; i < 100; i++) {
        RID rid(i, 0);
        tree_->Insert(i, rid);
    }

    auto results = tree_->RangeScan(20, 30);
    EXPECT_EQ(results.size(), 11); // 20,21,...,30

    for (auto &rid : results) {
        EXPECT_GE(rid.page_id, 20u);
        EXPECT_LE(rid.page_id, 30u);
    }
}

TEST_F(BPlusTreeTest, RangeScanEmpty) {
    for (int i = 0; i < 10; i++) {
        tree_->Insert(i, RID(i, 0));
    }
    auto results = tree_->RangeScan(100, 200);
    EXPECT_TRUE(results.empty());
}

TEST_F(BPlusTreeTest, KeysRemainSortedAfterSplits) {
    std::vector<int> keys = {5,3,8,1,9,2,7,4,6,0};
    for (int k : keys) tree_->Insert(k, RID(k, 0));

    auto results = tree_->RangeScan(0, 9);
    EXPECT_EQ(results.size(), 10);
}