#include <Parser.hh>

#include <Ast.hh>
#include <Lexer.hh>
#include <Token.hh>

#include <codegen/support/Stack.hh>

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

std::unique_ptr<ast::Node> create_expr(Op op, Stack<std::unique_ptr<ast::Node>> &operands) {
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

std::unique_ptr<ast::CallExpr> Parser::parse_call_expr(std::unique_ptr<ast::Symbol> &&name) {
    auto call_expr = std::make_unique<ast::CallExpr>(std::move(name));
    expect(TokenKind::LeftParen);
    while (m_lexer.peek().kind() != TokenKind::RightParen) {
        call_expr->add_arg(parse_expr());
        consume(TokenKind::Comma);
    }
    expect(TokenKind::RightParen);
    return call_expr;
}

std::unique_ptr<ast::Node> Parser::parse_expr() {
    Stack<std::unique_ptr<ast::Node>> operands;
    Stack<Op> operators;
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
            case TokenKind::Identifier: {
                auto symbol = std::make_unique<ast::Symbol>(std::string(expect(TokenKind::Identifier).text()));
                if (m_lexer.peek().kind() == TokenKind::LeftParen) {
                    operands.push(parse_call_expr(std::move(symbol)));
                } else {
                    operands.push(std::move(symbol));
                }
                break;
            }
            case TokenKind::IntLit:
                operands.push(std::make_unique<ast::IntegerLiteral>(expect(TokenKind::IntLit).number()));
                break;
            case TokenKind::LeftBrace: {
                operands.push(parse_block());
                break;
            }
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

std::unique_ptr<ast::YieldStmt> Parser::parse_yield_stmt() {
    if (!consume(TokenKind::KeywordYield)) {
        return {};
    }
    auto expr = parse_expr();
    expect(TokenKind::Semi);
    return std::make_unique<ast::YieldStmt>(std::move(expr));
}

std::unique_ptr<ast::Node> Parser::parse_stmt() {
    if (auto decl_stmt = parse_decl_stmt()) {
        return decl_stmt;
    }
    if (auto return_stmt = parse_return_stmt()) {
        return return_stmt;
    }
    if (auto yield_stmt = parse_yield_stmt()) {
        return yield_stmt;
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

std::unique_ptr<ast::Root> Parser::parse() {
    auto root = std::make_unique<ast::Root>();
    while (m_lexer.has_next()) {
        expect(TokenKind::KeywordFn);
        auto name = expect(TokenKind::Identifier);
        expect(TokenKind::LeftParen);
        auto function = std::make_unique<ast::FunctionDecl>(std::string(name.text()));
        while (m_lexer.peek().kind() != TokenKind::RightParen) {
            expect(TokenKind::KeywordLet);
            auto arg_name = expect(TokenKind::Identifier);
            function->add_arg(std::string(arg_name.text()));
            consume(TokenKind::Comma);
        }
        expect(TokenKind::RightParen);
        function->set_block(parse_block());
        root->add_function(std::move(function));
    }
    return root;
}
