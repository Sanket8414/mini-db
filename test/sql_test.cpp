#include <gtest/gtest.h>
#include "sql/lexer.h"
#include "sql/parser.h"
#include "sql/ast.h"

// ---- Lexer tests ----

TEST(LexerTest, TokenizeSelect) {
    Lexer lexer("SELECT * FROM users;");
    auto tokens = lexer.Tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::SELECT);
    EXPECT_EQ(tokens[1].type, TokenType::STAR);
    EXPECT_EQ(tokens[2].type, TokenType::FROM);
    EXPECT_EQ(tokens[3].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[3].value, "users");
    EXPECT_EQ(tokens[4].type, TokenType::SEMICOLON);
}

TEST(LexerTest, TokenizeInsert) {
    Lexer lexer("INSERT INTO t VALUES (1, 'Alice');");
    auto tokens = lexer.Tokenize();

    EXPECT_EQ(tokens[0].type, TokenType::INSERT);
    EXPECT_EQ(tokens[1].type, TokenType::INTO);
    EXPECT_EQ(tokens[4].type, TokenType::INT_LITERAL);
    EXPECT_EQ(tokens[4].value, "1");
    EXPECT_EQ(tokens[6].type, TokenType::STRING_LITERAL);
    EXPECT_EQ(tokens[6].value, "Alice");
}

TEST(LexerTest, CaseInsensitiveKeywords) {
    Lexer lexer("select * from users;");
    auto tokens = lexer.Tokenize();
    EXPECT_EQ(tokens[0].type, TokenType::SELECT);
    EXPECT_EQ(tokens[2].type, TokenType::FROM);
}

// ---- Parser tests ----

TEST(ParserTest, ParseCreateTable) {
    Lexer  lexer("CREATE TABLE users (id INT PRIMARY KEY, name VARCHAR(50));");
    Parser parser(lexer.Tokenize());
    auto   stmt = parser.ParseStatement();

    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->type, StmtType::CREATE_TABLE);

    auto *create = static_cast<CreateTableStmt *>(stmt.get());
    EXPECT_EQ(create->table_name, "users");
    EXPECT_EQ(create->columns.size(), 2);
    EXPECT_EQ(create->columns[0].name, "id");
    EXPECT_TRUE(create->columns[0].is_primary_key);
    EXPECT_EQ(create->columns[1].name, "name");
    EXPECT_EQ(create->columns[1].max_length, 50);
}

TEST(ParserTest, ParseInsert) {
    Lexer  lexer("INSERT INTO users VALUES (1, 'Bob');");
    Parser parser(lexer.Tokenize());
    auto   stmt = parser.ParseStatement();

    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->type, StmtType::INSERT);

    auto *insert = static_cast<InsertStmt *>(stmt.get());
    EXPECT_EQ(insert->table_name, "users");
    EXPECT_EQ(insert->values.size(), 2);
    EXPECT_EQ(insert->values[0].GetInt(), 1);
    EXPECT_EQ(insert->values[1].GetString(), "Bob");
}

TEST(ParserTest, ParseSelectStar) {
    Lexer  lexer("SELECT * FROM users;");
    Parser parser(lexer.Tokenize());
    auto   stmt = parser.ParseStatement();

    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->type, StmtType::SELECT);

    auto *select = static_cast<SelectStmt *>(stmt.get());
    EXPECT_TRUE(select->IsSelectAll());
    EXPECT_EQ(select->from_table, "users");
    EXPECT_EQ(select->where_clause, nullptr);
}

TEST(ParserTest, ParseSelectWithWhere) {
    Lexer  lexer("SELECT id, name FROM users WHERE id = 1;");
    Parser parser(lexer.Tokenize());
    auto   stmt = parser.ParseStatement();

    auto *select = static_cast<SelectStmt *>(stmt.get());
    EXPECT_FALSE(select->IsSelectAll());
    EXPECT_EQ(select->columns.size(), 2);
    EXPECT_NE(select->where_clause, nullptr);
}

TEST(ParserTest, InvalidSQLThrows) {
    Lexer  lexer("INVALID SQL;");
    Parser parser(lexer.Tokenize());
    EXPECT_THROW(parser.ParseStatement(), std::runtime_error);
}