#include "execution/filter.h"
#include <stdexcept>

Filter::Filter(std::unique_ptr<Executor> child,
               Expr                     *where_expr,
               const TableSchema        &schema)
    : child_(std::move(child)),
      where_expr_(where_expr),
      schema_(schema) {}

void Filter::Open() {
    child_->Open();
}

Tuple *Filter::Next() {
    Tuple *t;
    while ((t = child_->Next()) != nullptr) {
        if (Evaluate(where_expr_, *t)) {
            current_tuple_ = *t;
            return &current_tuple_;
        }
    }
    return nullptr;
}

void Filter::Close() {
    child_->Close();
}

bool Filter::Evaluate(Expr *expr, const Tuple &tuple) {
    Value result = EvaluateExpr(expr, tuple);
    if (result.IsNull() || !result.IsInt()) return false;
    return result.GetInt() != 0;
}

Value Filter::EvaluateExpr(Expr *expr, const Tuple &tuple) {
    if (expr->type == ExprType::LITERAL) {
        return static_cast<LiteralExpr *>(expr)->value;
    }

    if (expr->type == ExprType::COLUMN_REF) {
        auto *col_ref = static_cast<ColumnRefExpr *>(expr);
        int idx = col_ref->col_index;
        if (idx < 0)
            throw std::runtime_error("Filter: unbound column ref: "
                                     + col_ref->col_name);
        return tuple.GetValue(static_cast<uint16_t>(idx));
    }

    if (expr->type == ExprType::BINARY_EXPR) {
        auto *bin = static_cast<BinaryExpr *>(expr);

        // Short-circuit AND / OR
        if (bin->op == BinaryOp::AND) {
            Value l = EvaluateExpr(bin->left.get(), tuple);
            if (!l.IsNull() && l.IsInt() && l.GetInt() == 0)
                return Value(0);
            Value r = EvaluateExpr(bin->right.get(), tuple);
            if (!r.IsNull() && r.IsInt() && r.GetInt() == 0)
                return Value(0);
            return Value(1);
        }

        if (bin->op == BinaryOp::OR) {
            Value l = EvaluateExpr(bin->left.get(), tuple);
            if (!l.IsNull() && l.IsInt() && l.GetInt() != 0)
                return Value(1);
            Value r = EvaluateExpr(bin->right.get(), tuple);
            if (!r.IsNull() && r.IsInt() && r.GetInt() != 0)
                return Value(1);
            return Value(0);
        }

        Value l = EvaluateExpr(bin->left.get(), tuple);
        Value r = EvaluateExpr(bin->right.get(), tuple);

        // NULL propagation — any comparison with NULL is false
        if (l.IsNull() || r.IsNull()) return Value(0);

        switch (bin->op) {
            case BinaryOp::EQ:  return Value(l == r ? 1 : 0);
            case BinaryOp::NEQ: return Value(l != r ? 1 : 0);
            case BinaryOp::LT:  return Value(l <  r ? 1 : 0);
            case BinaryOp::GT:  return Value(l >  r ? 1 : 0);
            case BinaryOp::LTE: return Value(l <= r ? 1 : 0);
            case BinaryOp::GTE: return Value(l >= r ? 1 : 0);
            default: return Value(0);
        }
    }

    throw std::runtime_error("Filter: unknown expression type");
}