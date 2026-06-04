#include <gtest/gtest.h>
#include <filesystem>
#include "storage/disk_manager.h"
#include "storage/buffer_pool.h"
#include "catalog/catalog.h"
#include "execution/planner.h"
#include "execution/binder.h"
#include "sql/lexer.h"
#include "sql/parser.h"

class ExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_ = "test_exec.db";
        std::filesystem::remove(test_file_);
        dm_      = std::make_unique<DiskManager>(test_file_);
        dm_->AllocatePage();
        bp_      = std::make_unique<BufferPool>(32, *dm_);
        catalog_ = std::make_unique<Catalog>(*bp_);
        catalog_->Load();
        planner_ = std::make_unique<Planner>(*catalog_, *bp_);
        binder_  = std::make_unique<Binder>(*catalog_);
    }
    void TearDown() override {
        planner_.reset();
        binder_.reset();
        catalog_.reset();
        bp_.reset();
        dm_.reset();
        std::filesystem::remove(test_file_);
    }

    void ExecSQL(const std::string &sql) {
        Lexer  lexer(sql);
        Parser parser(lexer.Tokenize());
        auto   stmt = parser.ParseStatement();
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

    std::string                  test_file_;
    std::unique_ptr<DiskManager> dm_;
    std::unique_ptr<BufferPool>  bp_;
    std::unique_ptr<Catalog>     catalog_;
    std::unique_ptr<Planner>     planner_;
    std::unique_ptr<Binder>      binder_;
};

TEST_F(ExecutorTest, SeqScanReturnsAllRows) {
    ExecSQL("CREATE TABLE t (id INT PRIMARY KEY, val INT);");
    for (int i = 1; i <= 10; i++) {
        ExecSQL("INSERT INTO t VALUES (" + std::to_string(i)
                + ", " + std::to_string(i * 10) + ");");
    }

    Lexer  lexer("SELECT * FROM t;");
    Parser parser(lexer.Tokenize());
    auto   stmt   = parser.ParseStatement();
    auto  *select = static_cast<SelectStmt *>(stmt.get());
    binder_->BindSelect(*select);

    auto executor = planner_->PlanSelect(*select);
    executor->Open();

    int count = 0;
    while (executor->Next() != nullptr) count++;
    executor->Close();

    EXPECT_EQ(count, 10);
}

TEST_F(ExecutorTest, FilterReducesRows) {
    ExecSQL("CREATE TABLE t (id INT PRIMARY KEY, val INT);");
    for (int i = 1; i <= 10; i++) {
        ExecSQL("INSERT INTO t VALUES (" + std::to_string(i)
                + ", " + std::to_string(i) + ");");
    }

    Lexer  lexer("SELECT * FROM t WHERE val > 5;");
    Parser parser(lexer.Tokenize());
    auto   stmt   = parser.ParseStatement();
    auto  *select = static_cast<SelectStmt *>(stmt.get());
    binder_->BindSelect(*select);

    auto executor = planner_->PlanSelect(*select);
    executor->Open();
    int count = 0;
    while (executor->Next() != nullptr) count++;
    executor->Close();

    EXPECT_EQ(count, 5); // 6,7,8,9,10
}

TEST_F(ExecutorTest, IndexScanFindsExactRow) {
    ExecSQL("CREATE TABLE t (id INT PRIMARY KEY, name VARCHAR(20));");
    for (int i = 1; i <= 20; i++) {
        ExecSQL("INSERT INTO t VALUES (" + std::to_string(i)
                + ", 'name" + std::to_string(i) + "');");
    }

    Lexer  lexer("SELECT * FROM t WHERE id = 10;");
    Parser parser(lexer.Tokenize());
    auto   stmt   = parser.ParseStatement();
    auto  *select = static_cast<SelectStmt *>(stmt.get());
    binder_->BindSelect(*select);

    auto executor = planner_->PlanSelect(*select);
    executor->Open();

    int   count = 0;
    Tuple *t;
    while ((t = executor->Next()) != nullptr) {
        EXPECT_EQ(t->GetValue(0).GetInt(), 10);
        count++;
    }
    executor->Close();
    EXPECT_EQ(count, 1);
}