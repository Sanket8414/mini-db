#pragma once

#include <memory>
#include "sql/ast.h"
#include "execution/executor.h"
#include "execution/seq_scan.h"
#include "execution/index_scan.h"
#include "execution/filter.h"
#include "execution/projection.h"
#include "catalog/catalog.h"
#include "storage/buffer_pool.h"

// Planner — creates a physical execution plan from a bound AST
// Rule-based:
//   WHERE on primary key with = -> IndexScan
//   WHERE on primary key with range -> IndexScan (range)
//   Otherwise -> SeqScan + Filter
class Planner {
public:
    Planner(Catalog &catalog, BufferPool &bp);

    // Returns the root executor for a SELECT statement
    std::unique_ptr<Executor> PlanSelect(SelectStmt &stmt);

    // Execute INSERT directly (no executor needed)
    void ExecuteInsert(InsertStmt &stmt);

    // Execute CREATE TABLE directly
    void ExecuteCreate(CreateTableStmt &stmt);

private:
    Catalog    &catalog_;
    BufferPool &bp_;

    // Check if WHERE clause is a simple equality/range on primary key
    // Returns true + fills search_key if it's a pk equality: WHERE pk = x
    bool IsPKEquality(Expr *where_expr, const TableSchema &schema,
                      Value &out_key);

    // Returns true + fills low/high if it's a pk range scan
    bool IsPKRange(Expr *where_expr, const TableSchema &schema,
                   Value &out_low, Value &out_high);

    // Get the primary key value from an expression node
    Value ExtractLiteralValue(Expr *expr);
};