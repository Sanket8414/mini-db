#pragma once

#include "execution/executor.h"
#include "sql/ast.h"
#include "catalog/schema.h"
#include <memory>

// Filter — wraps another executor, only passes tuples where
// the WHERE expression evaluates to true
class Filter : public Executor {
public:
    Filter(std::unique_ptr<Executor> child,
           Expr                     *where_expr,
           const TableSchema        &schema);

    void   Open()  override;
    Tuple *Next()  override;
    void   Close() override;

private:
    std::unique_ptr<Executor> child_;
    Expr                     *where_expr_; // not owned
    TableSchema               schema_;

    // Evaluate a WHERE expression against a tuple
    // Returns true if the tuple passes the filter
    bool Evaluate(Expr *expr, const Tuple &tuple);

    // Evaluate a single expression node, return its Value
    Value EvaluateExpr(Expr *expr, const Tuple &tuple);
};