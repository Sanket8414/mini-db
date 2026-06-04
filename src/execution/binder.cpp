#include "execution/binder.h"
#include <stdexcept>

Binder::Binder(Catalog &catalog) : catalog_(catalog) {}

void Binder::BindSelect(SelectStmt &stmt) {
    if (!catalog_.TableExists(stmt.from_table))
        throw std::runtime_error("Binder: table not found: " + stmt.from_table);

    const TableSchema &schema = catalog_.GetTable(stmt.from_table);

    // Validate requested columns exist
    if (!stmt.IsSelectAll()) {
        for (auto &col_name : stmt.columns) {
            if (schema.GetColumnIndex(col_name) < 0)
                throw std::runtime_error("Binder: column not found: " + col_name);
        }
    }

    // Bind WHERE expression
    if (stmt.where_clause) {
        BindExpr(stmt.where_clause.get(), schema);
    }
}

void Binder::BindInsert(InsertStmt &stmt) {
    if (!catalog_.TableExists(stmt.table_name))
        throw std::runtime_error("Binder: table not found: " + stmt.table_name);

    const TableSchema &schema = catalog_.GetTable(stmt.table_name);

    if (stmt.values.size() != schema.columns.size())
        throw std::runtime_error("Binder: value count doesn't match column count");
}

void Binder::BindCreate(CreateTableStmt &stmt) {
    if (catalog_.TableExists(stmt.table_name))
        throw std::runtime_error("Binder: table already exists: "
                                 + stmt.table_name);
}

void Binder::BindExpr(Expr *expr, const TableSchema &schema) {
    if (expr == nullptr) return;

    if (expr->type == ExprType::COLUMN_REF) {
        auto *col_ref = static_cast<ColumnRefExpr *>(expr);
        int idx = schema.GetColumnIndex(col_ref->col_name);
        if (idx < 0)
            throw std::runtime_error("Binder: column not found: "
                                     + col_ref->col_name);
        col_ref->col_index = idx;
        return;
    }

    if (expr->type == ExprType::BINARY_EXPR) {
        auto *bin = static_cast<BinaryExpr *>(expr);
        BindExpr(bin->left.get(),  schema);
        BindExpr(bin->right.get(), schema);
    }
}