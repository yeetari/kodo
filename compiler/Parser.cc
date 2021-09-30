#include <Parser.hh>

#include <Ast.hh>
#include <Lexer.hh>
#include <Token.hh>

#include <codegen/support/Queue.hh>

#include <fmt/core.h>

#include <cstdlib>

namespace {

enum class Op {
    Add,
    Sub,
};

constexpr int precedence(Op op) {
    switch (op) {
    case Op::Add:
    case Op::Sub:
        return 1;
    }
}

constexpr int compare_op(Op op1, Op op2) {
    int p1 = precedence(op1);
    int p2 = precedence(op2);
    return p1 > p2 ? 1 : p1 < p2 ? -1 : 0;
}

std::unique_ptr<ast::Node> create_expr(Op op, Queue<std::unique_ptr<ast::Node>> &operands) {
    auto rhs = operands.pop();
    auto lhs = operands.pop();
    switch (op) {
    case Op::Add:
        return std::make_unique<ast::BinaryExpr>(ast::BinaryOp::Add, std::move(lhs), std::move(rhs));
    case Op::Sub:
        return std::make_unique<ast::BinaryExpr>(ast::BinaryOp::Sub, std::move(lhs), std::move(rhs));
    }
}

} // namespace

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
    Queue<std::unique_ptr<ast::Node>> operands;
    Queue<Op> operators;
    bool keep_parsing = true;
    while (keep_parsing) {
        std::optional<Op> op1;
        switch (m_lexer.peek().kind()) {
        case TokenKind::Plus:
            op1 = Op::Add;
            break;
        case TokenKind::Minus:
            op1 = Op::Sub;
            break;
        default:
            break;
        }
        if (!op1) {
            switch (m_lexer.peek().kind()) {
            case TokenKind::Identifier:
                operands.push(std::make_unique<ast::Symbol>(std::string(expect(TokenKind::Identifier).text())));
                break;
            case TokenKind::IntLit:
                operands.push(std::make_unique<ast::IntegerLiteral>(expect(TokenKind::IntLit).number()));
                break;
            default:
                keep_parsing = false;
                break;
            }
            continue;
        }
        m_lexer.next();
        while (!operators.empty()) {
            auto op2 = operators.peek();
            if (compare_op(*op1, op2) >= 0) {
                break;
            }
            auto op = operators.pop();
            operands.push(create_expr(op, operands));
        }
        operators.push(*op1);
    }
    while (!operators.empty()) {
        auto op = operators.pop();
        operands.push(create_expr(op, operands));
    }
    if (operands.size() != 1) {
        fmt::print("error: unfinished expression\n");
        std::exit(1);
    }
    return operands.pop();
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
