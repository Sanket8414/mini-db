#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include "common/types.h"
#include "common/value.h"

// ---- Expression nodes ----

enum class ExprType {
    LITERAL,
    COLUMN_REF,
    BINARY_EXPR,
};

enum class BinaryOp {
    EQ, NEQ, LT, GT, LTE, GTE, AND, OR
};

std::string BinaryOpToString(BinaryOp op);

struct Expr {
    ExprType type;
    virtual ~Expr() = default;
};

struct LiteralExpr : public Expr {
    Value value;
    explicit LiteralExpr(Value v) : value(std::move(v)) {
        type = ExprType::LITERAL;
    }
};

struct ColumnRefExpr : public Expr {
    std::string col_name;
    int         col_index = -1; // filled in by binder
    explicit ColumnRefExpr(const std::string &name) : col_name(name) {
        type = ExprType::COLUMN_REF;
    }
};

struct BinaryExpr : public Expr {
    std::unique_ptr<Expr> left;
    BinaryOp              op;
    std::unique_ptr<Expr> right;

    BinaryExpr(std::unique_ptr<Expr> l, BinaryOp op,
               std::unique_ptr<Expr> r)
        : left(std::move(l)), op(op), right(std::move(r)) {
        type = ExprType::BINARY_EXPR;
    }
};

// ---- Column definition (used in CREATE TABLE) ----

struct ColumnDef {
    std::string name;
    TypeId      type;
    uint32_t    max_length;     // for VARCHAR
    bool        is_primary_key;

    ColumnDef() : type(TypeId::INVALID), max_length(0), is_primary_key(false) {}
};

// ---- Statement nodes ----

enum class StmtType {
    CREATE_TABLE,
    INSERT,
    SELECT,
};

struct Statement {
    StmtType type;
    virtual ~Statement() = default;
};

struct CreateTableStmt : public Statement {
    std::string            table_name;
    std::vector<ColumnDef> columns;

    CreateTableStmt() { type = StmtType::CREATE_TABLE; }
};

struct InsertStmt : public Statement {
    std::string        table_name;
    std::vector<Value> values;

    InsertStmt() { type = StmtType::INSERT; }
};

struct SelectStmt : public Statement {
    std::vector<std::string>    columns;     // empty means SELECT *
    std::string                 from_table;
    std::unique_ptr<Expr>       where_clause; // nullptr means no WHERE

    bool IsSelectAll() const { return columns.empty(); }

    SelectStmt() { type = StmtType::SELECT; }
};