#include "sql/lexer.h"
#include <cctype>
#include <stdexcept>
#include <algorithm>

Lexer::Lexer(const std::string &input)
    : input_(input), pos_(0), line_(1), col_(1) {}

std::vector<Token> Lexer::Tokenize() {
    std::vector<Token> tokens;

    while (!IsAtEnd()) {
        SkipWhitespace();
        if (IsAtEnd()) break;

        char c = Current();

        if (c == '-' && Peek() == '-') {
            SkipLineComment();
            continue;
        }

        if (std::isalpha(c) || c == '_') {
            tokens.push_back(ReadIdentifierOrKeyword());
        } else if (std::isdigit(c)) {
            tokens.push_back(ReadIntLiteral());
        } else if (c == '\'') {
            tokens.push_back(ReadStringLiteral());
        } else {
            tokens.push_back(ReadOperator());
        }
    }

    tokens.emplace_back(TokenType::EOF_TOKEN, "", line_, col_);
    return tokens;
}

char Lexer::Current() const {
    if (IsAtEnd()) return '\0';
    return input_[pos_];
}

char Lexer::Peek(size_t offset) const {
    if (pos_ + offset >= input_.size()) return '\0';
    return input_[pos_ + offset];
}

char Lexer::Advance() {
    char c = input_[pos_++];
    if (c == '\n') { line_++; col_ = 1; }
    else           { col_++; }
    return c;
}

bool Lexer::IsAtEnd() const {
    return pos_ >= input_.size();
}

void Lexer::SkipWhitespace() {
    while (!IsAtEnd() && std::isspace(Current())) Advance();
}

void Lexer::SkipLineComment() {
    while (!IsAtEnd() && Current() != '\n') Advance();
}

Token Lexer::ReadIdentifierOrKeyword() {
    int    start_line = line_, start_col = col_;
    size_t start      = pos_;

    while (!IsAtEnd() && (std::isalnum(Current()) || Current() == '_')) {
        Advance();
    }

    std::string word = input_.substr(start, pos_ - start);

    // SQL keywords are case-insensitive — convert to upper for matching
    std::string upper = word;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    TokenType t = KeywordOrIdentifier(upper);
    // Preserve original case for identifiers, use upper for keywords
    return Token(t, t == TokenType::IDENTIFIER ? word : upper,
                 start_line, start_col);
}

Token Lexer::ReadIntLiteral() {
    int    start_line = line_, start_col = col_;
    size_t start      = pos_;
    while (!IsAtEnd() && std::isdigit(Current())) Advance();
    return Token(TokenType::INT_LITERAL,
                 input_.substr(start, pos_ - start),
                 start_line, start_col);
}

Token Lexer::ReadStringLiteral() {
    int start_line = line_, start_col = col_;
    Advance(); // consume opening '
    std::string result;
    while (!IsAtEnd()) {
        char c = Advance();
        if (c == '\'') {
            // SQL escape: '' means a literal single quote
            if (!IsAtEnd() && Current() == '\'') {
                result += '\'';
                Advance();
            } else {
                break; // end of string
            }
        } else {
            result += c;
        }
    }
    return Token(TokenType::STRING_LITERAL, result, start_line, start_col);
}

Token Lexer::ReadOperator() {
    int  start_line = line_, start_col = col_;
    char c = Advance();

    switch (c) {
        case '*': return Token(TokenType::STAR,      "*", start_line, start_col);
        case ',': return Token(TokenType::COMMA,     ",", start_line, start_col);
        case ';': return Token(TokenType::SEMICOLON, ";", start_line, start_col);
        case '(': return Token(TokenType::LPAREN,    "(", start_line, start_col);
        case ')': return Token(TokenType::RPAREN,    ")", start_line, start_col);
        case '=': return Token(TokenType::EQ,        "=", start_line, start_col);
        case '<':
            if (!IsAtEnd() && Current() == '=') {
                Advance();
                return Token(TokenType::LTE, "<=", start_line, start_col);
            }
            return Token(TokenType::LT, "<", start_line, start_col);
        case '>':
            if (!IsAtEnd() && Current() == '=') {
                Advance();
                return Token(TokenType::GTE, ">=", start_line, start_col);
            }
            return Token(TokenType::GT, ">", start_line, start_col);
        case '!':
            if (!IsAtEnd() && Current() == '=') {
                Advance();
                return Token(TokenType::NEQ, "!=", start_line, start_col);
            }
            break;
        default: break;
    }
    return Token(TokenType::UNKNOWN, std::string(1, c), start_line, start_col);
}

TokenType Lexer::KeywordOrIdentifier(const std::string &upper) const {
    if (upper == "SELECT")  return TokenType::SELECT;
    if (upper == "FROM")    return TokenType::FROM;
    if (upper == "WHERE")   return TokenType::WHERE;
    if (upper == "INSERT")  return TokenType::INSERT;
    if (upper == "INTO")    return TokenType::INTO;
    if (upper == "VALUES")  return TokenType::VALUES;
    if (upper == "CREATE")  return TokenType::CREATE;
    if (upper == "TABLE")   return TokenType::TABLE;
    if (upper == "PRIMARY") return TokenType::PRIMARY;
    if (upper == "KEY")     return TokenType::KEY;
    if (upper == "AND")     return TokenType::AND;
    if (upper == "OR")      return TokenType::OR;
    if (upper == "NOT")     return TokenType::NOT;
    if (upper == "INT")     return TokenType::INT_TYPE;
    if (upper == "VARCHAR") return TokenType::VARCHAR_TYPE;
    if (upper == "NULL")    return TokenType::NUll_KW;
    return TokenType::IDENTIFIER;
}