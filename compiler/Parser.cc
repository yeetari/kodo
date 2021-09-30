#include <Parser.hh>

#include <Ast.hh>
#include <Lexer.hh>
#include <Token.hh>

#include <fmt/core.h>

#include <cstdlib>

std::optional<Token> Parser::consume(TokenKind kind) {
    if (m_lexer.peek().kind() == kind) {
        return m_lexer.next();
    }
    return std::nullopt;
}

Token Parser::expect(TokenKind kind) {
    auto next = m_lexer.next();
    if (next.kind() != kind) {
        fmt::print("error: expected {} but got {}\n", Token::kind_string(kind), next.to_string());
        std::exit(1);
    }
    return next;
}

std::unique_ptr<ast::Node> Parser::parse_expr() {
    if (auto identifier = consume(TokenKind::Identifier)) {
        return std::make_unique<ast::Symbol>(std::string(identifier->text()));
    }
    return std::make_unique<ast::IntegerLiteral>(expect(TokenKind::IntLit).number());
}

std::unique_ptr<ast::DeclStmt> Parser::parse_decl_stmt() {
    if (!consume(TokenKind::KeywordLet)) {
        return {};
    }
    auto name = expect(TokenKind::Identifier);
    expect(TokenKind::Eq);
    auto expr = parse_expr();
    expect(TokenKind::Semi);
    return std::make_unique<ast::DeclStmt>(std::string(name.text()), std::move(expr));
}

std::unique_ptr<ast::ReturnStmt> Parser::parse_return_stmt() {
    if (!consume(TokenKind::KeywordReturn)) {
        return {};
    }
    auto expr = parse_expr();
    expect(TokenKind::Semi);
    return std::make_unique<ast::ReturnStmt>(std::move(expr));
}

std::unique_ptr<ast::Node> Parser::parse_stmt() {
    if (auto decl_stmt = parse_decl_stmt()) {
        return decl_stmt;
    }
    if (auto return_stmt = parse_return_stmt()) {
        return return_stmt;
    }
    fmt::print("error: expected a statement but got {}\n", m_lexer.next().to_string());
    std::exit(1);
}

std::unique_ptr<ast::Block> Parser::parse_block() {
    auto block = std::make_unique<ast::Block>();
    expect(TokenKind::LeftBrace);
    while (m_lexer.has_next() && m_lexer.peek().kind() != TokenKind::RightBrace) {
        block->add_stmt(parse_stmt());
    }
    expect(TokenKind::RightBrace);
    return block;
}

std::unique_ptr<ast::FunctionDecl> Parser::parse() {
    expect(TokenKind::KeywordFn);
    auto name = expect(TokenKind::Identifier);
    expect(TokenKind::LeftParen);
    expect(TokenKind::RightParen);
    auto function = std::make_unique<ast::FunctionDecl>(std::string(name.text()));
    function->set_block(parse_block());
    return function;
}
