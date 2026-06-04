#include "sql/ast.h"

std::string BinaryOpToString(BinaryOp op) {
    switch (op) {
        case BinaryOp::EQ:  return "=";
        case BinaryOp::NEQ: return "!=";
        case BinaryOp::LT:  return "<";
        case BinaryOp::GT:  return ">";
        case BinaryOp::LTE: return "<=";
        case BinaryOp::GTE: return ">=";
        case BinaryOp::AND: return "AND";
        case BinaryOp::OR:  return "OR";
        default:            return "?";
    }
}