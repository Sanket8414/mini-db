#include "sql/parser.h"
#include <stdexcept>

Parser::Parser(const std::vector<Token> &tokens)
    : tokens_(tokens), pos_(0) {}

// ---- Token navigation ----

const Token &Parser::Current() const { return tokens_[pos_]; }

const Token &Parser::Peek(size_t offset) const {
    size_t idx = pos_ + offset;
    if (idx >= tokens_.size()) return tokens_.back();
    return tokens_[idx];
}

const Token &Parser::Consume() {
    const Token &t = tokens_[pos_];
    if (!IsAtEnd()) pos_++;
    return t;
}

const Token &Parser::Expect(TokenType type, const std::string &msg) {
    if (!Check(type)) throw std::runtime_error(ParseError(msg));
    return Consume();
}

bool Parser::Check(TokenType type) const {
    return Current().type == type;
}

bool Parser::Match(TokenType type) {
    if (Check(type)) { Consume(); return true; }
    return false;
}

bool Parser::IsAtEnd() const {
    return Current().type == TokenType::EOF_TOKEN;
}

std::string Parser::ParseError(const std::string &msg) const {
    return "Parse error at line " + std::to_string(Current().line)
         + " col " + std::to_string(Current().col)
         + ": " + msg + " (got '" + Current().value + "')";
}

// ---- Top-level ----

std::unique_ptr<Statement> Parser::ParseStatement() {
    if (IsAtEnd()) return nullptr;

    if (Check(TokenType::CREATE)) return ParseCreateTable();
    if (Check(TokenType::INSERT)) return ParseInsert();
    if (Check(TokenType::SELECT)) return ParseSelect();

    throw std::runtime_error(ParseError("Expected CREATE, INSERT, or SELECT"));
}

// ---- CREATE TABLE ----

std::unique_ptr<CreateTableStmt> Parser::ParseCreateTable() {
    Expect(TokenType::CREATE, "Expected CREATE");
    Expect(TokenType::TABLE,  "Expected TABLE");

    auto stmt = std::make_unique<CreateTableStmt>();
    stmt->table_name = Expect(TokenType::IDENTIFIER,
                              "Expected table name").value;

    Expect(TokenType::LPAREN, "Expected '('");

    // Parse column definitions
    while (!Check(TokenType::RPAREN) && !IsAtEnd()) {
        stmt->columns.push_back(ParseColumnDef());
        if (!Match(TokenType::COMMA)) break;
    }

    Expect(TokenType::RPAREN,   "Expected ')'");
    Expect(TokenType::SEMICOLON,"Expected ';'");

    return stmt;
}

ColumnDef Parser::ParseColumnDef() {
    ColumnDef col;
    col.name = Expect(TokenType::IDENTIFIER, "Expected column name").value;

    if (Check(TokenType::INT_TYPE)) {
        Consume();
        col.type       = TypeId::INT;
        col.max_length = 0;
    } else if (Check(TokenType::VARCHAR_TYPE)) {
        Consume();
        Expect(TokenType::LPAREN, "Expected '(' after VARCHAR");
        col.max_length = static_cast<uint32_t>(
            std::stoi(Expect(TokenType::INT_LITERAL,
                             "Expected VARCHAR length").value));
        Expect(TokenType::RPAREN, "Expected ')'");
        col.type = TypeId::VARCHAR;
    } else {
        throw std::runtime_error(ParseError("Expected column type INT or VARCHAR"));
    }

    // Optional PRIMARY KEY
    if (Check(TokenType::PRIMARY)) {
        Consume();
        Expect(TokenType::KEY, "Expected KEY after PRIMARY");
        col.is_primary_key = true;
    }

    return col;
}

// ---- INSERT ----

std::unique_ptr<InsertStmt> Parser::ParseInsert() {
    Expect(TokenType::INSERT, "Expected INSERT");
    Expect(TokenType::INTO,   "Expected INTO");

    auto stmt = std::make_unique<InsertStmt>();
    stmt->table_name = Expect(TokenType::IDENTIFIER,
                              "Expected table name").value;

    Expect(TokenType::VALUES, "Expected VALUES");
    Expect(TokenType::LPAREN, "Expected '('");

    while (!Check(TokenType::RPAREN) && !IsAtEnd()) {
        stmt->values.push_back(ParseLiteralValue());
        if (!Match(TokenType::COMMA)) break;
    }

    Expect(TokenType::RPAREN,    "Expected ')'");
    Expect(TokenType::SEMICOLON, "Expected ';'");

    return stmt;
}

Value Parser::ParseLiteralValue() {
    if (Check(TokenType::INT_LITERAL)) {
        int32_t v = std::stoi(Consume().value);
        return Value(v);
    }
    if (Check(TokenType::STRING_LITERAL)) {
        return Value(Consume().value);
    }
    if (Check(TokenType::NUll_KW)) {
        Consume();
        return Value(); // NULL
    }
    throw std::runtime_error(ParseError("Expected a literal value"));
}

// ---- SELECT ----

std::unique_ptr<SelectStmt> Parser::ParseSelect() {
    Expect(TokenType::SELECT, "Expected SELECT");

    auto stmt = std::make_unique<SelectStmt>();

    // SELECT * or SELECT col1, col2, ...
    if (Match(TokenType::STAR)) {
        // columns stays empty — means SELECT *
    } else {
        stmt->columns.push_back(
            Expect(TokenType::IDENTIFIER, "Expected column name").value);
        while (Match(TokenType::COMMA)) {
            stmt->columns.push_back(
                Expect(TokenType::IDENTIFIER, "Expected column name").value);
        }
    }

    Expect(TokenType::FROM, "Expected FROM");
    stmt->from_table = Expect(TokenType::IDENTIFIER,
                              "Expected table name").value;

    // Optional WHERE
    if (Match(TokenType::WHERE)) {
        stmt->where_clause = ParseExpression();
    }

    Expect(TokenType::SEMICOLON, "Expected ';'");
    return stmt;
}

// ---- Expression parsing (recursive descent) ----
// Precedence (low to high): OR -> AND -> comparison -> primary

std::unique_ptr<Expr> Parser::ParseExpression() {
    auto left = ParseAnd();
    while (Check(TokenType::OR)) {
        Consume();
        auto right = ParseAnd();
        left = std::make_unique<BinaryExpr>(
            std::move(left), BinaryOp::OR, std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::ParseAnd() {
    auto left = ParseComparison();
    while (Check(TokenType::AND)) {
        Consume();
        auto right = ParseComparison();
        left = std::make_unique<BinaryExpr>(
            std::move(left), BinaryOp::AND, std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::ParseComparison() {
    auto left = ParsePrimary();

    if (Check(TokenType::EQ)  || Check(TokenType::NEQ) ||
        Check(TokenType::LT)  || Check(TokenType::GT)  ||
        Check(TokenType::LTE) || Check(TokenType::GTE)) {

        BinaryOp op = TokenToBinaryOp(Consume().type);
        auto right  = ParsePrimary();
        return std::make_unique<BinaryExpr>(
            std::move(left), op, std::move(right));
    }

    return left;
}

std::unique_ptr<Expr> Parser::ParsePrimary() {
    // Parenthesized expression
    if (Match(TokenType::LPAREN)) {
        auto expr = ParseExpression();
        Expect(TokenType::RPAREN, "Expected ')'");
        return expr;
    }

    // NULL literal
    if (Check(TokenType::NUll_KW)) {
        Consume();
        return std::make_unique<LiteralExpr>(Value());
    }

    // Integer literal
    if (Check(TokenType::INT_LITERAL)) {
        int32_t v = std::stoi(Consume().value);
        return std::make_unique<LiteralExpr>(Value(v));
    }

    // String literal
    if (Check(TokenType::STRING_LITERAL)) {
        return std::make_unique<LiteralExpr>(Value(Consume().value));
    }

    // Column reference
    if (Check(TokenType::IDENTIFIER)) {
        return std::make_unique<ColumnRefExpr>(Consume().value);
    }

    throw std::runtime_error(ParseError("Expected expression"));
}

BinaryOp Parser::TokenToBinaryOp(TokenType t) {
    switch (t) {
        case TokenType::EQ:  return BinaryOp::EQ;
        case TokenType::NEQ: return BinaryOp::NEQ;
        case TokenType::LT:  return BinaryOp::LT;
        case TokenType::GT:  return BinaryOp::GT;
        case TokenType::LTE: return BinaryOp::LTE;
        case TokenType::GTE: return BinaryOp::GTE;
        default: throw std::runtime_error("Not a binary operator");
    }
}