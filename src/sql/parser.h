#pragma once

#include <vector>
#include <memory>
#include <stdexcept>
#include "sql/lexer.h"
#include "sql/ast.h"

class Parser {
public:
    explicit Parser(const std::vector<Token> &tokens);

    // Parse one statement — returns nullptr at EOF
    std::unique_ptr<Statement> ParseStatement();

private:
    const std::vector<Token> &tokens_;
    size_t                    pos_;

    // Token navigation
    const Token &Current() const;
    const Token &Peek(size_t offset = 1) const;
    const Token &Consume();
    const Token &Expect(TokenType type, const std::string &msg);
    bool         Check(TokenType type) const;
    bool         Match(TokenType type);
    bool         IsAtEnd() const;

    // Statement parsers
    std::unique_ptr<CreateTableStmt> ParseCreateTable();
    std::unique_ptr<InsertStmt>      ParseInsert();
    std::unique_ptr<SelectStmt>      ParseSelect();

    // Expression parsers (recursive descent, handles precedence)
    std::unique_ptr<Expr> ParseExpression();  // OR level
    std::unique_ptr<Expr> ParseAnd();         // AND level
    std::unique_ptr<Expr> ParseComparison();  // = < > <= >= != level
    std::unique_ptr<Expr> ParsePrimary();     // literal, column, (expr)

    // Helpers
    Value          ParseLiteralValue();
    ColumnDef      ParseColumnDef();
    BinaryOp       TokenToBinaryOp(TokenType t);
    std::string    ParseError(const std::string &msg) const;
};