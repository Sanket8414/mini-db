#pragma once

#include "sql/ast.h"
#include "catalog/catalog.h"

// Binder — resolves column names in the AST against the catalog
// After binding, every ColumnRefExpr has col_index set correctly
// Throws if a table or column doesn't exist
class Binder {
public:
    explicit Binder(Catalog &catalog);

    void BindSelect(SelectStmt       &stmt);
    void BindInsert(InsertStmt       &stmt);
    void BindCreate(CreateTableStmt  &stmt);

private:
    Catalog &catalog_;

    // Resolve all column refs in an expression against a table schema
    void BindExpr(Expr *expr, const TableSchema &schema);
};