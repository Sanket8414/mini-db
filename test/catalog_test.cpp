#include <gtest/gtest.h>
#include <filesystem>
#include "catalog/catalog.h"
#include "storage/buffer_pool.h"
#include "storage/disk_manager.h"

class CatalogTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_ = "test_catalog.db";
        std::filesystem::remove(test_file_);
        dm_      = std::make_unique<DiskManager>(test_file_);
        dm_->AllocatePage(); // page 0 for catalog
        bp_      = std::make_unique<BufferPool>(16, *dm_);
        catalog_ = std::make_unique<Catalog>(*bp_);
    }
    void TearDown() override {
        catalog_.reset();
        bp_.reset();
        dm_.reset();
        std::filesystem::remove(test_file_);
    }
    std::string                  test_file_;
    std::unique_ptr<DiskManager> dm_;
    std::unique_ptr<BufferPool>  bp_;
    std::unique_ptr<Catalog>     catalog_;
};

TEST_F(CatalogTest, CreateAndGetTable) {
    TableSchema schema("users", {
        ColumnSchema("id",   TypeId::INT,     0,  true),
        ColumnSchema("name", TypeId::VARCHAR, 50, false),
    });
    catalog_->CreateTable(schema);

    EXPECT_TRUE(catalog_->TableExists("users"));
    TableSchema &s = catalog_->GetTable("users");
    EXPECT_EQ(s.name, "users");
    EXPECT_EQ(s.columns.size(), 2);
    EXPECT_EQ(s.columns[0].name, "id");
    EXPECT_TRUE(s.columns[0].is_primary_key);
}

TEST_F(CatalogTest, DuplicateTableThrows) {
    TableSchema schema("users", {
        ColumnSchema("id", TypeId::INT, 0, true)
    });
    catalog_->CreateTable(schema);
    EXPECT_THROW(catalog_->CreateTable(schema), std::runtime_error);
}

TEST_F(CatalogTest, GetNonExistentTableThrows) {
    EXPECT_THROW(catalog_->GetTable("nonexistent"), std::runtime_error);
}

TEST_F(CatalogTest, PersistAndReload) {
    TableSchema schema("products", {
        ColumnSchema("id",    TypeId::INT,     0,   true),
        ColumnSchema("price", TypeId::INT,     0,   false),
        ColumnSchema("name",  TypeId::VARCHAR, 100, false),
    });
    catalog_->CreateTable(schema);
    catalog_->Save();

    // Reload catalog from same buffer pool
    Catalog catalog2(*bp_);
    catalog2.Load();

    EXPECT_TRUE(catalog2.TableExists("products"));
    TableSchema &s = catalog2.GetTable("products");
    EXPECT_EQ(s.columns.size(), 3);
}

TEST_F(CatalogTest, ListTables) {
    catalog_->CreateTable(TableSchema("a", {ColumnSchema("id", TypeId::INT)}));
    catalog_->CreateTable(TableSchema("b", {ColumnSchema("id", TypeId::INT)}));
    catalog_->CreateTable(TableSchema("c", {ColumnSchema("id", TypeId::INT)}));

    auto tables = catalog_->ListTables();
    EXPECT_EQ(tables.size(), 3);
}