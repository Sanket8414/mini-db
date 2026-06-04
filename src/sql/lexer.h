#pragma once

#include <string>
#include <string_view>
#include <vector>

enum class TokenType {
    // Keywords
    SELECT, FROM, WHERE, INSERT, INTO, VALUES,
    CREATE, TABLE, PRIMARY, KEY, AND, OR, NOT,
    INT_TYPE, VARCHAR_TYPE, NUll_KW,
    // Symbols
    STAR, COMMA, SEMICOLON, LPAREN, RPAREN,
    // Operators
    EQ, NEQ, LT, GT, LTE, GTE,
    // Literals & identifiers
    INT_LITERAL, STRING_LITERAL, IDENTIFIER,
    // Control
    EOF_TOKEN, UNKNOWN,
};

struct Token {
    TokenType   type;
    std::string value;  // raw text of the token
    int         line;
    int         col;

    Token(TokenType t, std::string v, int line, int col)
        : type(t), value(std::move(v)), line(line), col(col) {}
};

class Lexer {
public:
    explicit Lexer(const std::string &input);

    // Tokenize the entire input and return all tokens
    std::vector<Token> Tokenize();

private:
    std::string input_;
    size_t      pos_;
    int         line_;
    int         col_;

    char   Current() const;
    char   Peek(size_t offset = 1) const;
    char   Advance();
    bool   IsAtEnd() const;
    void   SkipWhitespace();
    void   SkipLineComment();

    Token  ReadIdentifierOrKeyword();
    Token  ReadIntLiteral();
    Token  ReadStringLiteral();
    Token  ReadOperator();

    TokenType KeywordOrIdentifier(const std::string &word) const;
};