#pragma once

#include <Ast.hh>
#include <Token.hh>

#include <memory>
#include <optional>

class Lexer;

class Parser {
    Lexer &m_lexer;

    std::optional<Token> consume(TokenKind kind);
    Token expect(TokenKind kind);

    std::unique_ptr<ast::CallExpr> parse_call_expr(std::unique_ptr<ast::Symbol> &&name);
    std::unique_ptr<ast::MatchExpr> parse_match_expr();
    std::unique_ptr<ast::Node> parse_expr();
    std::unique_ptr<ast::DeclStmt> parse_decl_stmt();
    std::unique_ptr<ast::ReturnStmt> parse_return_stmt();
    std::unique_ptr<ast::YieldStmt> parse_yield_stmt();
    std::unique_ptr<ast::Node> parse_stmt();
    std::unique_ptr<ast::Block> parse_block();
    std::unique_ptr<ast::Type> parse_type();

public:
    explicit Parser(Lexer &lexer) : m_lexer(lexer) {}

    std::unique_ptr<ast::Root> parse();
};
