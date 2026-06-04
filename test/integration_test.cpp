#include <gtest/gtest.h>
#include <filesystem>
#include "storage/disk_manager.h"
#include "storage/buffer_pool.h"
#include "catalog/catalog.h"
#include "execution/planner.h"
#include "execution/binder.h"
#include "sql/lexer.h"
#include "sql/parser.h"

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_ = "test_integration.db";
        std::filesystem::remove(test_file_);
        Init();
    }
    void TearDown() override {
        Shutdown();
        std::filesystem::remove(test_file_);
    }

    void Init() {
        dm_      = std::make_unique<DiskManager>(test_file_);
        if (dm_->GetNumPages() == 0) dm_->AllocatePage();
        bp_      = std::make_unique<BufferPool>(64, *dm_);
        catalog_ = std::make_unique<Catalog>(*bp_);
        catalog_->Load();
        binder_  = std::make_unique<Binder>(*catalog_);
        planner_ = std::make_unique<Planner>(*catalog_, *bp_);
    }

    void Shutdown() {
        planner_.reset();
        binder_.reset();
        catalog_.reset();
        bp_.reset();
        dm_.reset();
    }

    void Exec(const std::string &sql) {
        Lexer  lexer(sql);
        Parser parser(lexer.Tokenize());
        auto   stmt = parser.ParseStatement();
        if (!stmt) return;

        if (stmt->type == StmtType::CREATE_TABLE) {
            auto *s = static_cast<CreateTableStmt *>(stmt.get());
            binder_->BindCreate(*s);
            planner_->ExecuteCreate(*s);
        } else if (stmt->type == StmtType::INSERT) {
            auto *s = static_cast<InsertStmt *>(stmt.get());
            binder_->BindInsert(*s);
            planner_->ExecuteInsert(*s);
        }
    }

    int CountRows(const std::string &sql) {
        Lexer  lexer(sql);
        Parser parser(lexer.Tokenize());
        auto   stmt   = parser.ParseStatement();
        auto  *select = static_cast<SelectStmt *>(stmt.get());
        binder_->BindSelect(*select);
        auto executor = planner_->PlanSelect(*select);
        executor->Open();
        int count = 0;
        while (executor->Next() != nullptr) count++;
        executor->Close();
        return count;
    }

    std::string                  test_file_;
    std::unique_ptr<DiskManager> dm_;
    std::unique_ptr<BufferPool>  bp_;
    std::unique_ptr<Catalog>     catalog_;
    std::unique_ptr<Binder>      binder_;
    std::unique_ptr<Planner>     planner_;
};

TEST_F(IntegrationTest, CreateInsertSelect) {
    Exec("CREATE TABLE users (id INT PRIMARY KEY, name VARCHAR(50));");
    Exec("INSERT INTO users VALUES (1, 'Alice');");
    Exec("INSERT INTO users VALUES (2, 'Bob');");
    Exec("INSERT INTO users VALUES (3, 'Charlie');");

    EXPECT_EQ(CountRows("SELECT * FROM users;"), 3);
}

TEST_F(IntegrationTest, SelectWhereFilters) {
    Exec("CREATE TABLE t (id INT PRIMARY KEY, val INT);");
    for (int i = 1; i <= 20; i++)
        Exec("INSERT INTO t VALUES (" + std::to_string(i)
             + ", " + std::to_string(i) + ");");

    EXPECT_EQ(CountRows("SELECT * FROM t WHERE val > 15;"), 5);
    EXPECT_EQ(CountRows("SELECT * FROM t WHERE id = 10;"), 1);
}

TEST_F(IntegrationTest, DataPersistsAfterReopen) {
    Exec("CREATE TABLE users (id INT PRIMARY KEY, name VARCHAR(50));");
    Exec("INSERT INTO users VALUES (1, 'Alice');");
    Exec("INSERT INTO users VALUES (2, 'Bob');");
    catalog_->Save();
    bp_->FlushAllPages();

    // Simulate closing and reopening the database
    Shutdown();
    Init();

    EXPECT_TRUE(catalog_->TableExists("users"));
    EXPECT_EQ(CountRows("SELECT * FROM users;"), 2);
}

TEST_F(IntegrationTest, InsertManyRowsAcrossPages) {
    Exec("CREATE TABLE t (id INT PRIMARY KEY, val INT);");
    for (int i = 1; i <= 500; i++)
        Exec("INSERT INTO t VALUES (" + std::to_string(i)
             + ", " + std::to_string(i * 2) + ");");

    EXPECT_EQ(CountRows("SELECT * FROM t;"), 500);
}

TEST_F(IntegrationTest, RangeScanViaIndex) {
    Exec("CREATE TABLE t (id INT PRIMARY KEY, name VARCHAR(10));");
    for (int i = 1; i <= 100; i++)
        Exec("INSERT INTO t VALUES (" + std::to_string(i) + ", 'x');");

    EXPECT_EQ(CountRows(
        "SELECT * FROM t WHERE id >= 50 AND id <= 60;"), 11);
}